/**
 * @file usb_device_tinyusb_egress_interface_no_lwip.c
 * @author Vincent Ho (mch79@cam.ac.uk)
 * @brief 
 * @version 0.1
 * @date 2024-02-09
 * 
 * @copyright NA
 * 
 */

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/multicore.h"

#include <stdio.h>
#include <time.h>
#include <inttypes.h>

#include <SEGGER_RTT.h>

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "queue.h"
#include "semphr.h"
#include "message_buffer.h"

#include "bsp/board.h"
#include "tusb.h"

#include <pb_encode.h>
#include <pb_decode.h>
#include <pb_common.h>
#include <periodic_sampler_stream_egress.pb.h>

/******************************************************
 * This C project showcases a robust serial communication system with USB between a 
 * host (such as a laptop running a pyserial script) and a Raspberry Pi Pico W, configured as 
 * a USB CDC (Communication Device Class) device. The Pico W is running FreeRTOS using TinyUSB 
 * as the usb stack and nanopb to decode and parse commands from the host.
 *****************************************************/

// 128*4 = 512 bytes
// (recommended min stack size for each task)
#define STACK_SIZE         128*4


// Increase stack size when debug log is enabled
#define USBD_STACK_SIZE    (3*configMINIMAL_STACK_SIZE/2) * (CFG_TUSB_DEBUG ? 2 : 1)

// Stack size for the CDC task
#define CDC_STACK_SIZE     configMINIMAL_STACK_SIZE

// Stack size for the cdc ingress task
// It has to be greater than the size of the recv_buf in task
// Right now uint8_t recv_buf[256], so it has to be greater than 256
#define CDC_INGRESS_STACK_SIZE      256*2

// Stack size for the cdc engress task
// It has to be greater than the size of the msg_buf in task
// Right now uint8_t msg_buf[256], so it has to be greater than 256
#define CDC_EGRESS_STACK_SIZE      256*2

// Stack size for the periodic sampler task
#define PERIODIC_SAMPLER_STACK_SIZE     256

// Egress msg buffer size in bytes
#define EGRESS_MSG_BUF_SIZE  256*10

// Egress stream buffer size in bytes
#define EGRESS_STREAM_BUF_SIZE 256*10

// Extra LEDs to show status
// #define LED_1       28
// #define LED_2       17

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTOTYPES
//--------------------------------------------------------------------+

/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */

enum  {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 1000,
  BLINK_SUSPENDED = 2500,
};


// LED blinky timer handler
TimerHandle_t blinky_tm = NULL;

// USB device task handle
TaskHandle_t usbd_handle_c0 = NULL; 

// CDC egress task handle
TaskHandle_t cdc_egress_handle_c1 = NULL;

// CDC ingress task handle
TaskHandle_t cdc_ingress_handle_c0 = NULL;

// Notification receiver task handle
TaskHandle_t notif_recv_handle_c1 = NULL;

// Message producer task handle
TaskHandle_t msg_producer_handle_c1 = NULL;

// Periodic sampler task handle
TaskHandle_t periodic_sampler_handle_c1 = NULL;

// Host msg buffer handle
MessageBufferHandle_t host_msg_buf_handle = NULL;

// Egress msg buffer handle
// MessageBufferHandle_t egress_msg_buf_handle = NULL;

// Egress stream buffer handle
StreamBufferHandle_t egress_stream_buf_handle = NULL;

// Repeating timer for periodic sampler
repeating_timer_t periodic_sampler_timer;

// For testing
int32_t bytes_produced = 0;
int32_t bytes_sent_to_msg_buf = 0;
int32_t bytes_recv_from_msg_buf = 0;
int32_t bytes_sent_to_host = 0;

// For measuring time
uint64_t start = 0;
uint64_t end = 0;
uint64_t execution_time = 0;

// For keeping cdc connection status
bool cdc_term_connected = 0;

// For keeping track of whether a periodic sampler is active
bool active_periodic_sampler = 0;

// For keeping track of the element sent in periodic_sampler_cd(), used for testing
uint32_t sampler_counter = 0;


// Core 0 tasks
static void usb_device_task_c0(void *param);
// static void cdc_task_c0(void *param);
static void cdc_ingress_task_c0(void *param);

// Core 1 tasks
static void cdc_egress_task_c1(void *param);
static void periodic_sampler_task_c1(void *param);

// Local callback functions
static void led_blinky_cb(TimerHandle_t xTimer);
static bool periodic_sampler_cb(struct repeating_timer *t);

// TinyUSB callback functions
void tud_mount_cb(void);
void tud_umount_cb(void);
void tud_suspend_cb(bool remote_wakeup_en);
void tud_resume_cb(void);
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts);
void tud_cdc_rx_cb(uint8_t itf);
void tud_cdc_tx_complete_cb(uint8_t itf);

// Nanopb callback functions
bool nanopb_cb_host_to_device_msg_decode(pb_istream_t *stream, const pb_field_t *field, void **arg);
bool nanopb_cb_encode_string(pb_ostream_t *stream, const pb_field_t *field, void * const *arg);
bool nanopb_cb_encode_fixed32(pb_ostream_t *stream, const pb_field_t *field, void * const *arg);
bool nanopb_cb_encode_fixed64(pb_ostream_t *stream, const pb_field_t *field, void * const *arg);
bool nanopb_cb_encode_repeatedstring(pb_ostream_t *stream, const pb_field_t *field, void * const *arg);
bool nanopb_cb_print_int32(pb_istream_t *stream, const pb_field_t *field, void **arg);
bool nanopb_cb_print_string(pb_istream_t *stream, const pb_field_t *field, void **arg);

int main(void)
{
    SEGGER_RTT_Init();
    board_init();
  
    if (cyw43_arch_init())
    {
        SEGGER_RTT_printf(0, "Wi-Fi init failed.\n");
        printf("Wi-Fi init failed.\n");
    }else
    {
        SEGGER_RTT_printf(0, "Wi-Fi init suceeded.\n");
        printf("Wi-Fi init suceeded.\n");
    }

    // Initialise LED_1
    // gpio_init(LED_1);
    // gpio_set_dir(LED_1, GPIO_OUT);

    // Initialise LED_2
    // gpio_init(LED_2);
    // gpio_set_dir(LED_2, GPIO_OUT);

    // Turn off both LED 1 & 2 to start
    // gpio_put(LED_1, 0);
    // gpio_put(LED_2, 0);

    // Start with LED ON
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
  
    /*
    // Create a sw timer for blinky
    blinky_tm = xTimerCreate(NULL, pdMS_TO_TICKS(BLINK_NOT_MOUNTED), pdTRUE, NULL, led_blinky_cb);
    assert (blinky_tm != NULL);
    */
  
    // Create a task for tinyusb device stack task
    assert(xTaskCreate(usb_device_task_c0, "usbd_c0", USBD_STACK_SIZE, NULL, configMAX_PRIORITIES-1, &(usbd_handle_c0)) == pdPASS);
    assert(usbd_handle_c0 != NULL);
    // Configure the usb device task affinity mask, it can only run on core 0
    UBaseType_t uxCoreAffinityMask = ((1<<0));
    vTaskCoreAffinitySet(usbd_handle_c0, uxCoreAffinityMask);

    /*
    // Create a task for tinyusb to parse cdc messages
    assert(xTaskCreate(cdc_task_c0, "cdc_c0", CDC_STACK_SIZE, NULL, configMAX_PRIORITIES-2, &(cdc_handle_c0)) == pdPASS);
    assert(cdc_handle_c0 != NULL);
    // Configure the cdc task affinity mask, it can only run on core 0
    uxCoreAffinityMask = ((1<<0));
    vTaskCoreAffinitySet(cdc_handle_c0, uxCoreAffinityMask);
    */

    // Create a cdc ingress task to handle host-to-device messages 
    // cdc ingress task should have higher priority over egress task to ensure host-to-device messages parsing is prioritised
    assert(xTaskCreate(cdc_ingress_task_c0, "cdc_ingress_c0", CDC_INGRESS_STACK_SIZE, NULL, configMAX_PRIORITIES-1, &(cdc_ingress_handle_c0)) == pdPASS);
    assert(cdc_ingress_handle_c0 != NULL);
    // Configure the cdc ingress task affinity mask, it can only run on core 0
    uxCoreAffinityMask = ((1<<0));
    vTaskCoreAffinitySet(cdc_ingress_handle_c0, uxCoreAffinityMask);
    // Since this task can only be woken up through tud_cdc_rx_cb(), it is suspended on creation
    vTaskSuspend(cdc_ingress_handle_c0);

    // Create a task for cdc egress task to output data back to host
    assert(xTaskCreate(cdc_egress_task_c1, "cdc_egress_c1", CDC_EGRESS_STACK_SIZE, NULL, configMAX_PRIORITIES-1, &(cdc_egress_handle_c1)) == pdPASS);
    assert(cdc_egress_handle_c1 != NULL);
    // Configure the usb device task affinity mask, it can only run on core 1
    uxCoreAffinityMask = ((1<<1));
    vTaskCoreAffinitySet(cdc_egress_handle_c1, uxCoreAffinityMask);


    // Create a task on core 1 to set up periodic sampler and listen for commands from core 0
    assert(xTaskCreate(periodic_sampler_task_c1, "periodic_sampler_c1", PERIODIC_SAMPLER_STACK_SIZE, NULL, configMAX_PRIORITIES-1, &(periodic_sampler_handle_c1)) == pdPASS);
    assert(periodic_sampler_handle_c1 != NULL);
    // Configure the periodic sampler task affinity mask, it can only run on core 1
    uxCoreAffinityMask = ((1<<1));
    vTaskCoreAffinitySet(periodic_sampler_handle_c1, uxCoreAffinityMask);

    // Create a message buffer to store egress messages
    // egress_msg_buf_handle = xMessageBufferCreate(EGRESS_MSG_BUF_SIZE);
    // assert (egress_msg_buf_handle != NULL);

    // Create a stream buffer to store egress messages
    egress_stream_buf_handle = xStreamBufferCreate(EGRESS_STREAM_BUF_SIZE, 64);
    assert (egress_stream_buf_handle != NULL);

    // Start sw timer
    // assert(xTimerStart(blinky_tm, 0) == pdPASS);

    // Start scheduler
    vTaskStartScheduler();
    while (true) {};
}

//--------------------------------------------------------------------+
// TinyUSB device callbacks
//--------------------------------------------------------------------+
// Invoked when device is mounted
void tud_mount_cb(void)
{
    // xTimerChangePeriod(blinky_tm, pdMS_TO_TICKS(BLINK_MOUNTED), 0);
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
    // xTimerChangePeriod(blinky_tm, pdMS_TO_TICKS(BLINK_NOT_MOUNTED), 0);
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
    (void) remote_wakeup_en;
    // xTimerChangePeriod(blinky_tm, pdMS_TO_TICKS(BLINK_SUSPENDED), 0);
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
    /*
    if (tud_mounted())
    {
        xTimerChangePeriod(blinky_tm, pdMS_TO_TICKS(BLINK_MOUNTED), 0);
    }else
    {
        xTimerChangePeriod(blinky_tm, pdMS_TO_TICKS(BLINK_NOT_MOUNTED), 0);
    }
    */
}

// Invoked when cdc line state changed, e.g. connected/disconnected
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
    (void) itf;
    (void) rts;

    if (dtr)
    {
        // Terminal connected
        cdc_term_connected = 1;
        SEGGER_RTT_printf(0, "cdc term connected!\n");
    }else
    {
        // Terminal disconnected
        cdc_term_connected = 0;
        SEGGER_RTT_printf(0, "cdc term disconnected!\n\n");
        // If periodic sampler is active, stop it
        if (active_periodic_sampler)
        {
            if (cancel_repeating_timer(&(periodic_sampler_timer)))
            {
                active_periodic_sampler = 0;
                SEGGER_RTT_printf(0, "Cancelled periodic sampler.\n");
                printf("Cancelled periodic sampler.\n");
                // Reset sampler_counter for next callback
                sampler_counter = 0;
            }else
            {
                SEGGER_RTT_printf(0, "ERROR : Failed to cancel periodic sampler.\n");
                printf("ERROR : Failed to cancel periodic sampler.\n");
            }
        }
    }

}

// Invoked when cdc received new data
void tud_cdc_rx_cb(uint8_t itf)
{
    (void) itf;
    
    // Wake up cdc_ingress_task_c0 to process incoming data
    vTaskResume(cdc_ingress_handle_c0);

}

/*
// Invoked when a cdc tx is complete and therefore spaces becomes available in TX buffer
void tud_cdc_tx_complete_cb(uint8_t itf)
{
    (void) itf;

    // Notify cdc_egress_task_c1 to put more data into TX buffer and force a context switch to be evaluated
    // The cdc_egress_task_c1 will be unblocked as quickly as possible this way
    portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
    xTaskNotifyFromISR(cdc_egress_handle_c1, 1, eSetValueWithOverwrite, NULL);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
*/


/*
//--------------------------------------------------------------------+
// Local functions
//--------------------------------------------------------------------+
// The function checks the length of a host_to_device_msg and copies the content into msg_buffer.
// An entire message is then sent to the message buffer for decoding.
static void parse_host_to_device_msg_c0(void)
{
    // This is the msg buffer where we store our msg temporarily
    uint8_t msg_buffer[256];

    int32_t msg_buf_sent_bytes = 0;

    // Read the first byte which indicates the msg length
    int32_t msg_length = tud_cdc_read_char();
    SEGGER_RTT_printf(0, "Host to Device Message length = %d.\n", msg_length);
    printf("Message length = %d.\n", msg_length);

    if (msg_length != -1)
    {
        for (int i = 0; i < msg_length; i++)
        {
            msg_buffer[i] = tud_cdc_read_char();
        }
        // Copy the msg_buffer into the host_msg_buf for decoding
        msg_buf_sent_bytes = xMessageBufferSend(host_msg_buf_handle, msg_buffer, msg_length, pdMS_TO_TICKS(10));

        if (msg_buf_sent_bytes != 0)
        {
            SEGGER_RTT_printf(0, "Copied %d bytes into host_msg_buf.\n", msg_buf_sent_bytes);
            printf("Copied %d bytes into host_msg_buf.\n", msg_buf_sent_bytes);
        }else
        {
            SEGGER_RTT_printf(0, "ERROR : Failed to copy data to host_msg_buf.\n");
            printf("ERROR : Failed to copy data to host_msg_buf.\n");
        }
        
    }
}
*/


//--------------------------------------------------------------------+
// Nanopb callback functions
//--------------------------------------------------------------------+
/* The callback below is a message-level callback which is called before each
 * oneof message is decoded. It is used to set the pb_callback_t callbacks inside
 * the oneof message. The reason we need this is that different submessages share
 * storage inside oneof union, and before we know the message type we can't set
 * the callbacks without overwriting each other.
 */
bool nanopb_cb_host_to_device_msg_decode(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
    /* Print the prefix field before the submessages.
     * This also demonstrates how to access the top level message fields
     * from callbacks.
     */
    HostToDeviceMessage *topmsg = field->message;

    if (field->tag == HostToDeviceMessage_stop_periodic_sampler_msg_tag)
    {
        SEGGER_RTT_printf(0, "Got stop_periodic_sampler_msg.\n");
        // printf("Got stop_periodic_sampler_msg.\n");
        StopPeriodicSamplerMessage *msg = (StopPeriodicSamplerMessage*)field->pData;
        // msg->str1.funcs.decode = nanopb_cb_print_string;
        // msg->str1.arg = "str1 : %s \n";
    }
    else if (field->tag == HostToDeviceMessage_set_periodic_sampler_msg_tag)
    {
        SEGGER_RTT_printf(0, "Got set_periodic_sampler_msg.\n");
        // printf("Got set_periodic_sampler_msg.\n");
        SetPeriodicSamplerMessage *msg = (SetPeriodicSamplerMessage*)field->pData;
        // msg2->str1.funcs.decode = nanopb_cb_print_string;
        // msg2->str1.arg = "str1 : %s\n";
    }else
    {
        SEGGER_RTT_printf(0, "ERROR : Unknown field->tag in nanopb_msg_callback.\n");
        // printf("ERROR : Unknown field->tag in nanopb_msg_callback.\n");
        return false;
    }

    /* Once we return true, pb_dec_submessage() will go on to decode the
     * submessage contents. But if we want, we can also decode it ourselves
     * above and leave stream->bytes_left at 0 value, inhibiting automatic
     * decoding.
     */
    return true;
}

/* nanopb encode callbacks */
// Callback to encode a nanopb string
bool nanopb_cb_encode_string(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
    // Dereference arg
    const char *str = (const char*) (*arg);
    
    if (!pb_encode_tag_for_field(stream, field))
        return false;
    
    return pb_encode_string(stream, (uint8_t *) str, strlen(str));
}

// Callback to encode a nanopb int32
bool nanopb_cb_encode_int32(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
    if (!pb_encode_tag_for_field(stream, field))
        return false;
    
    return pb_encode_varint(stream, 42);
}

// Callback to encode a nanopb fixed32
bool nanopb_cb_encode_fixed32(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
    uint32_t value = 42;

    if (!pb_encode_tag_for_field(stream, field))
        return false;
    
    return pb_encode_fixed32(stream, &value);
}

// Callback to encode a nanopb fixed64
bool nanopb_cb_encode_fixed64(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
    uint64_t value = 42;

    if (!pb_encode_tag_for_field(stream, field))
        return false;
    
    return pb_encode_fixed64(stream, &value);
}

// Callback to encode nanopb repeated string
bool nanopb_cb_encode_repeatedstring(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
    char *str[4] = {"Hello world!", "", "Test", "Test2"};
    int i;
    
    for (i = 0; i < 4; i++)
    {
        if (!pb_encode_tag_for_field(stream, field))
            return false;
        
        if (!pb_encode_string(stream, (uint8_t*)str[i], strlen(str[i])))
            return false;
    }
    return true;
}

/* nanopb decode callbacks */
// Callback to print nanopb int32
bool nanopb_cb_print_int32(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
    uint64_t value;
    if (!pb_decode_varint(stream, &value))
        return false;

    printf((char*)*arg, (int)value);
    return true;
}

// Callback to print nanopb string
bool nanopb_cb_print_string(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
    SEGGER_RTT_printf(0, "Decoding print string...\n");
    // printf("Decoding print string...\n");
    uint8_t buffer[1024] = {0};
    
    /* We could read block-by-block to avoid the large buffer... */
    if (stream->bytes_left > sizeof(buffer) - 1)
        return false;
    
    if (!pb_read(stream, buffer, stream->bytes_left))
        return false;
    
    /* Print the string, in format comparable with protoc --decode.
     * Format comes from the arg defined in main().
     */
    printf((char*)*arg, buffer);
    return true;
}


//--------------------------------------------------------------------+
// TinyUSB device stack task (Core 0)
//--------------------------------------------------------------------+
static void usb_device_task_c0(void *param)
{
    (void) param;
    tud_init(BOARD_TUD_RHPORT);

    while (true)
    {
        tud_task();
    }
}

/*
//--------------------------------------------------------------------+
// USB CDC Task (Core 0)
//--------------------------------------------------------------------+
static void cdc_task_c0(void *param)
{
    while (true)
    {

        if (tud_cdc_connected())
        {
            if (tud_cdc_available())
            {
                parse_host_to_device_msg_c0();
                tud_cdc_read_flush();
            }
        }
    }
}
*/

//--------------------------------------------------------------------+
// Periodic sampler task (Core 1)
//--------------------------------------------------------------------+
static void periodic_sampler_task_c1(void *param)
{
    // Create a alarm pool on core 1, using hardware alarm 0
    alarm_pool_t *alarm_pool = alarm_pool_create(0U, 1U);
    while (true)
    {

        uint32_t notificationvalue = ulTaskNotifyTake( pdTRUE, portMAX_DELAY);
        if (notificationvalue == 0U)
        {
            if (active_periodic_sampler)
            {
                // Cancel any repeating timer
                if (cancel_repeating_timer(&(periodic_sampler_timer)))
                {
                    active_periodic_sampler = 0;
                    SEGGER_RTT_printf(0, "Cancelled periodic sampler.\n");
                    // Reset the egress_msg_buf to ensure that nothing can get pulled into tx cdc fifo
                    // assert(xMessageBufferReset(egress_msg_buf_handle) == pdPASS);
                    // Reset the egress_stream_buf to ensure that nothing can get pulled into tx cdc fifo
                    assert(xStreamBufferReset(egress_stream_buf_handle) == pdPASS);
                    // Flush the tx cdc fifo to ensure nothing gets sent
                    tud_cdc_write_clear();
                    // Notify cdc_ingress_task_c0 that periodic sampler has been cancelled, hence
                    // core 0 can send EOS sequence safety
                    xTaskNotify(cdc_ingress_handle_c0, 1U, eSetValueWithOverwrite);
                    
                    // printf("Cancelled periodic sampler.\n");
                }
            }
        }else
        {
            // Convert the unsigned into signed to be passed in as an argument
            // Maximum sampling frequency is 25000Hz, this is determined through experimentation.
            // The upper limit is determined by the execution time of tud_cdc_write()
            int64_t delay_us = (notificationvalue >= 40U) ? -(int64_t)notificationvalue : -40;
            // Set up a repeating timer
            // if delay_us > 0, then this is the delay between one callback ending and the next starting; 
            // if delay_us < 0, then this is the negative of the time between the starts of the callbacks.
            if (!active_periodic_sampler)
            {
                // Set up repeating periodic sampler timer
                if (!alarm_pool_add_repeating_timer_us(alarm_pool, delay_us, periodic_sampler_cb, NULL, &(periodic_sampler_timer)))
                {
                    SEGGER_RTT_printf(0, "ERROR : Failed to add periodic sampler.\n");
                    // printf("ERROR : Failed to add periodic sampler.\n");
                }else
                {
                    active_periodic_sampler = 1;

                    SEGGER_RTT_printf(0, "Added periodic sampler repeating timer with samping period = %lld microseconds\n", delay_us);
                    // printf("Added periodic sampler repeating timer with samping period = %lld microseconds\n", delay_us);
                }
            }
        }
    }

}

//--------------------------------------------------------------------+
// Notification receiver task (Core 1)
//--------------------------------------------------------------------+
/*
static void notif_recv_task_c1(void *param)
{
    while (true)
    {
        // Wait for the next notification value if cdc is connected
        if (tud_cdc_connected())
        {
            uint32_t notificationvalue = ulTaskNotifyTake( pdTRUE, portMAX_DELAY);
            if (notificationvalue == 1U)
            {
                printf("Receive notif %d \n", notificationvalue);
                printf("Resuming msg_producer task...\n");
                vTaskResume(msg_producer_handle_c1);
            }else if (notificationvalue == 0U)
            {
                printf("Recieve notif %d \n", notificationvalue);
                printf("Suspending msg_producer task...\n");
                vTaskSuspend(msg_producer_handle_c1);
            }else
            {
                printf("Unknown notif %d \n", notificationvalue);
            }
        }

    }
}
*/


//--------------------------------------------------------------------+
// CDC egress task (Core 1)
//--------------------------------------------------------------------+
static void cdc_egress_task_c1(void *param)
{
    // bool led_state = 0;
    int32_t bytes_recv = 0;
    uint8_t msg_buf[256];
    while (true)
    {
        // IMPORTANT
        // The code within the while(true){} should be kept as minimal as possible to poll the
        // stream buffer as quick as possible.
        // Using a minimal while(true){} allows a maximum sampling frequency of 25kHz to be achieved.

        // Fetch item from egress stream buf
        // SEGGER_RTT_printf(0, "Executing cdc_egress_task_c1.\n");
        // bytes_recv = xStreamBufferReceive(egress_stream_buf_handle, msg_buf, sizeof(msg_buf), portMAX_DELAY);
        bytes_recv = xStreamBufferReceive(egress_stream_buf_handle, msg_buf, sizeof(msg_buf), 0);
        // bytes_recv_from_msg_buf += bytes_recv;
        if (bytes_recv != 0)
        {
            // SEGGER_RTT_printf(0, "Took %d bytes from stream buf.\n", bytes_recv);
            // Write to cdc
            // int32_t bytes_written_into_cdc_tx_fifo = tud_cdc_write(msg_buf, bytes_recv);
            tud_cdc_write(msg_buf, bytes_recv);
            // Check that every byte from egress_msg_buf has been written into cdc tx fifo
            // assert(bytes_recv == bytes_written_into_cdc_tx_fifo);
            // bytes_sent_to_host += bytes_written_into_cdc_tx_fifo;

            // The existence of a task notification indicates that cdc has completed a transmission, hence it is
            // ready to accept more data
            // ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            // SEGGER_RTT_printf(0, "CDC TX complete.");
        }
        
        /*
        if (tud_cdc_connected())
        {
            // Fetch item from egress msg buf
            // bytes_recv = xMessageBufferReceive(egress_msg_buf_handle, msg_buf, sizeof(msg_buf), 0);
            // Fetch item from egress stream buf
            SEGGER_RTT_printf(0, "Executing cdc_egress_task_c1.\n");
            bytes_recv = xStreamBufferReceive(egress_stream_buf_handle, msg_buf, sizeof(msg_buf), 0);
            bytes_recv_from_msg_buf += bytes_recv;
            if (bytes_recv != 0)
            {
                SEGGER_RTT_printf(0, "Took %d bytes from stream buf.\n", bytes_recv);
                // Write to cdc
                int32_t bytes_written_into_cdc_tx_fifo = tud_cdc_write(msg_buf, bytes_recv);
                // Check that every byte from egress_msg_buf has been written into cdc tx fifo
                assert(bytes_recv == bytes_written_into_cdc_tx_fifo);
                bytes_sent_to_host += bytes_written_into_cdc_tx_fifo;

                // The existence of a task notification indicates that cdc has completed a transmission, hence it is
                // ready to accept more data
                // ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
                SEGGER_RTT_printf(0, "CDC TX complete.");
            }
        }
        */
            
    }
}


//--------------------------------------------------------------------+
// CDC ingress task (Core 0)
//--------------------------------------------------------------------+
static void cdc_ingress_task_c0(void *param)
{
    // Decode HostToDeviceMessage from CDC RX FIFO
    HostToDeviceMessage msg = HostToDeviceMessage_init_zero;
    // Set up the decode callbacks for contents in the oneof payload
    msg.cb_payload.funcs.decode = nanopb_cb_host_to_device_msg_decode;

    // recv buffer, each message has maximum size of 256 bytes
    uint8_t recv_buf[256];
    // int32_t recv_bytes = 0;
    while (true)
    {
        // Since this task is woken up by tud_cdc_rx_cb(), there is guranteed to have content
        // in CDC RX FIFO
        // Read the first byte which indicates the msg length
        int32_t msg_length = tud_cdc_read_char();
        SEGGER_RTT_printf(0, "Host to Device Message length = %d.\n", msg_length);

        if (msg_length != -1)
        {
            for (int i = 0; i < msg_length; i++)
            {
                recv_buf[i] = tud_cdc_read_char();
            }
        } 

        pb_istream_t stream = pb_istream_from_buffer(recv_buf, msg_length);

        bool status = pb_decode(&stream, HostToDeviceMessage_fields, &msg); 
        if (status)
        {
            SEGGER_RTT_printf(0, "Host to Device Message decode succeeded.\n");
            // Check the fields of message after decode and act accordingly
            if (msg.which_payload == HostToDeviceMessage_stop_periodic_sampler_msg_tag)
            {
                // Send task notification to periodic_sampler_task_c1 task to stop periodic sampling
                xTaskNotify(periodic_sampler_handle_c1, 0U, eSetValueWithOverwrite);

                // Wait for core 1 to indicate that it has stopped the periodic sampler timer and flush egress_msg_buf
                // before sending the EOS sequence
                uint32_t notificationvalue = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

                if (notificationvalue == 1U)
                {
                    // Send the EOS sequence
                    uint8_t msg_buf[10];
                    // Send 10 bytes of 255 to indicate end of stream back to host
                    memset(msg_buf, UINT8_MAX, sizeof(msg_buf));

                    SEGGER_RTT_printf(0, "TX FIFO available = %" PRIu32 "bytes.\n", tud_cdc_available());
                    // Flush the cdc tx fifo to ensure stop acknowledge can be sent back to host
                    bool cdc_write_cleared = tud_cdc_write_clear();
                    uint32_t bytes_written = tud_cdc_write(msg_buf, sizeof(msg_buf));
                    // When the number of bytes transmitted is small, they will hang around
                    // in the TX FIFO and accumulate before a critical mass is reached for
                    // Tinyusb to send all of them with a bulk transfer, we want to transmit
                    // instantly, so force send with tud_cdc_write_flush()
                    uint32_t bytes_written_after_flush = tud_cdc_write_flush();

                    // Make sure the ack message is transmitted
                    SEGGER_RTT_printf(0, "End of stream sequence sent. tud_cdc_write() = %" PRIu32 " bytes.\n", bytes_written);
                    SEGGER_RTT_printf(0, "tud_cdc_write_flush() = %" PRIu32 " bytes.\n", bytes_written_after_flush);
                    assert(bytes_written == sizeof(msg_buf) || bytes_written_after_flush == sizeof(msg_buf));
                    // printf("Ack_stop_periodic_sampler_msg sent. Msg length = %" PRIu32 "\n", bytes_written);

                    // Reset the sampler_counter
                    sampler_counter = 0;
                }else
                {
                    SEGGER_RTT_printf(0, "ERROR : notificationvalue for stop acknowledge from core 1 is not 1U.\n");
                }
                
            }else if (msg.which_payload == HostToDeviceMessage_set_periodic_sampler_msg_tag)
            {
                uint8_t msg_buf[32];
                pb_ostream_t stream;
                uint32_t notificationvalue = msg.payload.set_periodic_sampler_msg.sampling_period;
                // Create acknowledge message
                DeviceToHostMessage msg = DeviceToHostMessage_init_zero;
                msg.payload.ack_set_periodic_sampler_msg.ack = 1;
                msg.which_payload = DeviceToHostMessage_ack_set_periodic_sampler_msg_tag;
                stream = pb_ostream_from_buffer(msg_buf, sizeof(msg_buf));
                // Send acknowledge message back to host
                if (!pb_encode_ex(&stream, DeviceToHostMessage_fields, &msg, PB_ENCODE_DELIMITED))
                {
                    SEGGER_RTT_printf(0, "ERROR : DeviceToHostMessage encode failed.\n");
                    printf("ERROR : DeviceToHostMessage encode failed.\n");
                }
                // Flush the cdc tx fifo to ensure that set acknowledge can be sent back to host
                tud_cdc_write_clear();
                uint32_t bytes_written = tud_cdc_write(msg_buf, stream.bytes_written);
                // When the number of bytes transmitted is small, they will hang around
                // in the TX FIFO and accumulate before a critical mass is reached for
                // Tinyusb to send all of them with a bulk transfer, we want to transmit
                // instantly, so force send with tud_cdc_write_flush()
                bytes_written = tud_cdc_write_flush();

                // Make sure the ack message is transmitted
                assert(bytes_written == stream.bytes_written);
                SEGGER_RTT_printf(0, "Ack_set_periodic_sampler_msg sent. Msg length = %" PRIu32 "\n", bytes_written);
                printf("Ack_set_periodic_sampler_msg sent. Msg length = %" PRIu32 "\n", bytes_written);

                // Send task notification to periodic_sampler_task_c1 task to set periodic sampler
                xTaskNotify(periodic_sampler_handle_c1, notificationvalue, eSetValueWithOverwrite);

            }
        }else
        {
            SEGGER_RTT_printf(0, "ERROR : Nanopb message decode failure.\n");
        }
        // Suspend task after a host-to-device message has been parsed to free up execution time
        // for cdc_egress_task_c0
        vTaskSuspend(NULL);
    }
}

//--------------------------------------------------------------------+
// Local callback functions
//--------------------------------------------------------------------+
// Callback for blinky_tm sw timer
static void led_blinky_cb(TimerHandle_t xTimer)
{
    (void) xTimer;
    static bool led_state = false;
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_state);
    led_state = 1 - led_state;
    /*
    // Periodically write something to tud to check async python read
    if (tud_cdc_connected())
    {
        char msg_buf[16];
        if (led_state)
        {
            strncpy(msg_buf, "Hi", sizeof(msg_buf) - 1);
        }else
        {
            strncpy(msg_buf, "World", sizeof(msg_buf) - 1);
        }
        char *msg_buf = "Hello World!";
        printf("Wrote %d bytes to serial in leb_blinky_cb().\n", tud_cdc_write(&msg_buf, sizeof(msg_buf)));
        // Force sent is required for small number of bytes, otherwise it will sit
        // around in the TX FIFO until critical mass is reached before Tinyusb send
        // them in bulk
        printf("Force sent %d bytes to serial in led_blinky_cd().\n", tud_cdc_write_flush());
    }
    */

}

// Callback executed when the repeating periodic sampler timer expires
static bool periodic_sampler_cb(struct repeating_timer *t)
{
    uint16_t msg_buf[24];
    // Printf for now
    // SEGGER_RTT_printf(0, "Periodic sampling executing, start timestamp = %" PRIu64 "\n", time_us_64());
    // printf("Periodic sampling executed, timestamp = %" PRIu64 "\n", time_us_64());
    
    // char msg_buf[13]="Hello World!\n";

    // Copy data into the msg_buf according to current value of sampler_counter
    for (int i = 0; i < 24; i++)
    {
        msg_buf[i] = (uint16_t)(sampler_counter + i);
    }
    // Increment the sampler_counter for the next callback
    sampler_counter += 24;

    // Put content into egress_msg_buf for sending back to host
    // uint32_t bytes_written = xMessageBufferSendFromISR(egress_msg_buf_handle, &msg_buf, sizeof(msg_buf), 0);
    // Put content into egress_stream_buf for sending back to host
    uint32_t bytes_written = xStreamBufferSendFromISR(egress_stream_buf_handle, &msg_buf, sizeof(msg_buf), NULL);
    if (bytes_written != sizeof(msg_buf))
    {
        SEGGER_RTT_printf(0, "ERROR : bytes dropped due to full egress stream buf. TX FIFO avail = %" PRIu32 "bytes.\n", tud_cdc_write_available());
    }
    /*
    else
    {
        SEGGER_RTT_printf(0, "Wrote %" PRIu32 " bytes into stream buffer from periodic_sampler_cb().\n", bytes_written);
    }
    */
    // Check that the entire msg has been written into MessageBuffer
    assert(bytes_written == sizeof(msg_buf));
    // SEGGER_RTT_printf(0, "Wrote %d bytes to device in periodic_sampler_cb().\n", bytes_written);
    // SEGGER_RTT_printf(0, "Periodic sampling executed, end timestamp = %" PRIu64 "\n", time_us_64());
    // printf("Wrote %d bytes to device in periodic_sampler_cb().\n", bytes_written);
    // assert(xMessageBufferSendFromISR(egress_msg_buf_handle, msg_buf, sizeof(msg_buf), 0) != 0);

    return true;
}