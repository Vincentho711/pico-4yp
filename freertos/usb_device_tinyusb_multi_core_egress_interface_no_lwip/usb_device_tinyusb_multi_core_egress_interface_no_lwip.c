/**
 * @file usb_device_tinyusb_egress_interface_no_lwip.c
 * @author Vincent Ho (mch79@cam.ac.uk)
 * @brief 
 * @version 0.1
 * @date 2024-02-01
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
#include <egress_multi_core.pb.h>

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

// Stack size for the host msg decode task
// It has to be greater than the size of the recv_buf in task
// Right now uint8_t recv_buf[256], so it has to be greater than 256
#define HOST_MSG_DECODE_STACK_SIZE      256*2

// Stack size for the notification receiver task
#define NOTIF_RECV_STACK_SIZE           256

// Stack size for the message producer task
// It has to be greater than the maximum msg bytes
#define MSG_PROD_STACK_SIZE             256*2

// Host msg buffer size in bytes
#define HOST_MSG_BUF_SIZE    256*4

// Egress msg buffer size in bytes
#define EGRESS_MSG_BUF_SIZE  256*10

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
TaskHandle_t usbd_handle = NULL;

// CDC print out task handle
TaskHandle_t cdc_print_out_handle = NULL;

// Notification receiver task handle
TaskHandle_t notif_recv_handle = NULL;

// Message producer task handle
TaskHandle_t msg_producer_handle = NULL;

// Host msg buffer handle
MessageBufferHandle_t host_msg_buf_handle = NULL;

// Egress msg buffer handle
MessageBufferHandle_t egress_msg_buf_handle = NULL;

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

void led_blinky_cb(TimerHandle_t xTimer);
static void usb_device_task(void *param);
static void cdc_task(void *param);
static void notif_recv_task(void *param);
static void cdc_print_out_task(void *param);
static void message_producer_task(void *param);
static void host_msg_buf_decode_task(void *param);

static OneOfMessage construct_26_bytes_msg(void);
static OneOfMessage construct_49_bytes_msg(void);
static OneOfMessage construct_98_bytes_msg(void);
static OneOfMessage construct_183_bytes_msg(void);
static AdcMessage construct_adc_msg(void);

static void parse_host_msg(void);
static void decode_oneof_message_streaming(void);

bool nanopb_msg_callback(pb_istream_t *stream, const pb_field_t *field, void **arg);


bool nanopb_cb_encode_string(pb_ostream_t *stream, const pb_field_t *field, void * const *arg);
bool nanopb_cb_print_int32(pb_istream_t *stream, const pb_field_t *field, void **arg);
bool nanopb_cb_print_string(pb_istream_t *stream, const pb_field_t *field, void **arg);

int main(void)
{
    board_init();
  
    if (cyw43_arch_init())
    {
      printf("Wi-Fi init failed.\n");
    }else
    {
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
  
    // Create a sw timer for blinky
    blinky_tm = xTimerCreate(NULL, pdMS_TO_TICKS(BLINK_NOT_MOUNTED), pdTRUE, NULL, led_blinky_cb);
    assert (blinky_tm != NULL);
  
    // Create a task for tinyusb device stack task
    assert(xTaskCreate(usb_device_task, "usbd", USBD_STACK_SIZE, NULL, configMAX_PRIORITIES-1, &(usbd_handle)) == pdPASS);
    assert(usbd_handle != NULL);
    // Configure the usb device task affinity mask, it can only run on core 0
    UBaseType_t uxCoreAffinityMask = ((1<<0));
    // Set the affinity mask to task
    vTaskCoreAffinitySet(usbd_handle, uxCoreAffinityMask);

    // Create a task for tinyusb cdc print out task
    assert(xTaskCreate(cdc_print_out_task, "cdc_print_out", CDC_STACK_SIZE, NULL, configMAX_PRIORITIES-2, &(cdc_print_out_handle)) == pdPASS);
    assert(cdc_print_out_handle != NULL);
    // Configure the usb device task affinity mask, it can only run on core 0
    uxCoreAffinityMask = ((1<<0));
    // Set the affinity mask to task
    vTaskCoreAffinitySet(cdc_print_out_handle, uxCoreAffinityMask);

    // Create a task for receiving notification from core 0
    assert(xTaskCreate(notif_recv_task, "notif_recv", NOTIF_RECV_STACK_SIZE, NULL, configMAX_PRIORITIES-1, &(notif_recv_handle)) == pdPASS);
    assert(notif_recv_handle != NULL);
    // Configure the notif recv task affinity mask, it can only run on core 1
    uxCoreAffinityMask = ((1<<1));
    // Set the affinity mask to task
    vTaskCoreAffinitySet(notif_recv_handle, uxCoreAffinityMask);

    // Create a task for producing nanopb message
    assert(xTaskCreate(message_producer_task, "msg_prod", MSG_PROD_STACK_SIZE, NULL, configMAX_PRIORITIES-2, &(msg_producer_handle)) == pdPASS);
    assert(msg_producer_handle != NULL);
    // Configure the msg producer task affinity mask, it can only run on core 1
    uxCoreAffinityMask = ((1<<1));
    // Set the affinity mask to task
    vTaskCoreAffinitySet(msg_producer_handle, uxCoreAffinityMask);
    // By default, this task is suspended
    vTaskSuspend(msg_producer_handle);

    // Create a message buffer to store egress messages
    host_msg_buf_handle = xMessageBufferCreate(HOST_MSG_BUF_SIZE);
    assert (host_msg_buf_handle != NULL);

    // Create a message buffer to store egress messages
    egress_msg_buf_handle = xMessageBufferCreate(EGRESS_MSG_BUF_SIZE);
    assert (egress_msg_buf_handle != NULL);

    // Create a task to decode host messages from message buffer
    // Disabled in this test
    // assert(xTaskCreate(host_msg_buf_decode_task, "host_msg_decode", HOST_MSG_DECODE_STACK_SIZE, NULL, configMAX_PRIORITIES-3, NULL) == pdPASS);

    // Start sw timer
    assert(xTimerStart(blinky_tm, 0) == pdPASS);

    // Start scheduler
    vTaskStartScheduler();
    while (true) {};
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
    xTimerChangePeriod(blinky_tm, pdMS_TO_TICKS(BLINK_MOUNTED), 0);
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{

    xTimerChangePeriod(blinky_tm, pdMS_TO_TICKS(BLINK_NOT_MOUNTED), 0);
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
    (void) remote_wakeup_en;
    xTimerChangePeriod(blinky_tm, pdMS_TO_TICKS(BLINK_SUSPENDED), 0);
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
    if (tud_mounted())
    {
        xTimerChangePeriod(blinky_tm, pdMS_TO_TICKS(BLINK_MOUNTED), 0);
    }else
    {
        xTimerChangePeriod(blinky_tm, pdMS_TO_TICKS(BLINK_NOT_MOUNTED), 0);
    }
}


//--------------------------------------------------------------------+
// Local functions
//--------------------------------------------------------------------+

// Construct a message with type MsgType1, the message is a 26 bytes long message
static OneOfMessage construct_26_bytes_msg(void)
{

    OneOfMessage msg = OneOfMessage_init_zero;

    msg.lucky_number = 10;
    msg.str.arg = "Hello";
    msg.str.funcs.encode = &nanopb_cb_encode_string;
    msg.payload.msg1.str1.arg = "This is msg1";
    msg.payload.msg1.str1.funcs.encode = &nanopb_cb_encode_string;
    msg.which_payload = OneOfMessage_msg1_tag;
    return msg;
}

// Construct a message with type MsgType1, the message is a 49 bytes long message
static OneOfMessage construct_49_bytes_msg(void)
{

    OneOfMessage msg = OneOfMessage_init_zero;

    msg.lucky_number = 10;
    msg.str.arg = "Hello! Hello!";
    msg.str.funcs.encode = &nanopb_cb_encode_string;
    msg.payload.msg1.str1.arg = "This is msg1. This is msg1.";
    msg.payload.msg1.str1.funcs.encode = &nanopb_cb_encode_string;
    msg.which_payload = OneOfMessage_msg1_tag;
    return msg;
}

// Construct a message with type MsgType1, the message is a 98 bytes long message
static OneOfMessage construct_98_bytes_msg(void)
{

    OneOfMessage msg = OneOfMessage_init_zero;

    msg.lucky_number = 10;
    msg.str.arg = "Hello! Hello! Hello! Hello! Hello!";
    msg.str.funcs.encode = &nanopb_cb_encode_string;
    msg.payload.msg1.str1.arg = "This is msg1. This is msg1. This is msg1. This is msg1.";
    msg.payload.msg1.str1.funcs.encode = &nanopb_cb_encode_string;
    msg.which_payload = OneOfMessage_msg1_tag;
    return msg;
}

// Construct a message with type MsgType1, the message is a 183 bytes long message
static OneOfMessage construct_183_bytes_msg(void)
{

    OneOfMessage msg = OneOfMessage_init_zero;

    msg.lucky_number = 10;
    msg.str.arg = "Hello! Hello! Hello! Hello! Hello! Hello! Hello! Hello! Hello!";
    msg.str.funcs.encode = &nanopb_cb_encode_string;
    msg.payload.msg1.str1.arg = "This is msg1. This is msg1. This is msg1. This is msg1. This is msg1. This is msg1. This is msg1. This is msg1.";
    msg.payload.msg1.str1.funcs.encode = &nanopb_cb_encode_string;
    msg.which_payload = OneOfMessage_msg1_tag;
    return msg;
}

// Construct a message with type MsgType2 using nanopb
static OneOfMessage construct_msg2(void)
{
    OneOfMessage msg = OneOfMessage_init_zero;

    msg.lucky_number = 10;
    msg.str.arg = "World!";
    msg.str.funcs.encode = &nanopb_cb_encode_string;
    msg.payload.msg2.str1.arg = "This is msg2.";
    msg.payload.msg2.str1.funcs.encode = &nanopb_cb_encode_string;
    msg.which_payload = OneOfMessage_msg2_tag;
    return msg;
}

// Construct adc message
static AdcMessage construct_adc_msg(void)
{
    AdcMessage msg = AdcMessage_init_zero;
    msg.timestamp = 12345678;

    /*
    // total bytes = 30
    msg.ch1_val = 1111;
    msg.ch2_val = 2222;
    msg.ch3_val = 3333;
    msg.ch4_val = 4444;
    msg.ch5_val = 5555;
    msg.ch6_val = 6666;
    msg.ch7_val = 7777;
    msg.ch8_val = 8888;
    */

    // total bytes = 46
    msg.ch1_val = 11111111;
    msg.ch2_val = 22222222;
    msg.ch3_val = 33333333;
    msg.ch4_val = 44444444;
    msg.ch5_val = 55555555;
    msg.ch6_val = 66666666;
    msg.ch7_val = 77777777;
    msg.ch8_val = 88888888;

    /*
    // total bytes = 46
    msg.ch1_val = 999999999;
    msg.ch2_val = 999999999;
    msg.ch3_val = 999999999;
    msg.ch4_val = 999999999;
    msg.ch5_val = 999999999;
    msg.ch6_val = 999999999;
    msg.ch7_val = 999999999;
    msg.ch8_val = 999999999;
    */
    return msg;
}

// The function checks the length of the msg and copies the content into msg_buffer.
// An entire message is then sent to the message buffer for decoding.
static void parse_host_msg(void)
{
    // This is the msg buffer where we store our msg temporarily
    uint8_t msg_buffer[256];

    int32_t msg_buf_sent_bytes = 0;

    // Read the first byte which indicates the msg length
    int32_t msg_length = tud_cdc_read_char();
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
            printf("Copied %d into host_msg_buf.\n", msg_buf_sent_bytes);
        }else
        {
            printf("Failed to copy data to host_msg_buf.\n");
        }
        
    }
}

/* The callback below is a message-level callback which is called before each
 * oneof message is decoded. It is used to set the pb_callback_t callbacks inside
 * the oneof message. The reason we need this is that different submessages share
 * storage inside oneof union, and before we know the message type we can't set
 * the callbacks without overwriting each other.
 */
bool nanopb_msg_callback(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
    /* Print the prefix field before the submessages.
     * This also demonstrates how to access the top level message fields
     * from callbacks.
     */
    OneOfMessage *topmsg = field->message;
    printf("lucky_number: %d\n", (int)topmsg->lucky_number);
    topmsg->str.funcs.decode = nanopb_cb_print_string;
    topmsg->str.arg = "str : %s\n";


    if (field->tag == OneOfMessage_msg1_tag)
    {
        printf("Got MsgType1.\n");
        MsgType1 *msg1 = (MsgType1*)field->pData;
        msg1->str1.funcs.decode = nanopb_cb_print_string;
        msg1->str1.arg = "str1 : %s \n";
    }
    else if (field->tag == OneOfMessage_msg2_tag)
    {
        printf("Got MsgType2.\n");
        MsgType2 *msg2 = (MsgType2*)field->pData;
        msg2->str1.funcs.decode = nanopb_cb_print_string;
        msg2->str1.arg = "str1 : %s\n";
    }
    else if (field->tag == OneOfMessage_msg3_tag)
    {
        printf("Got MsgType3.\n");
        MsgType3 *msg3 = (MsgType3*)field->pData;
        msg3->str1.funcs.decode = nanopb_cb_print_string;
        msg3->str1.arg = "str1 : %s\n";
        msg3->str2.funcs.decode = nanopb_cb_print_string;
        msg3->str2.arg = "str2 : %s\n";
        msg3->str3.funcs.decode = nanopb_cb_print_string;
        msg3->str3.arg = "str3 : %s\n";
        msg3->str4.funcs.decode = nanopb_cb_print_string;
        msg3->str4.arg = "str4 : %s\n";
        msg3->str5.funcs.decode = nanopb_cb_print_string;
        msg3->str5.arg = "str5 : %s\n";
        msg3->str6.funcs.decode = nanopb_cb_print_string;
        msg3->str6.arg = "str6 : %s\n";
        msg3->str7.funcs.decode = nanopb_cb_print_string;
        msg3->str7.arg = "str7 : %s\n";
        msg3->str8.funcs.decode = nanopb_cb_print_string;
        msg3->str8.arg = "str8 : %s\n";

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
    printf("Decoding print string...\n");
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
// TinyUSB device stack task
//--------------------------------------------------------------------+
static void usb_device_task(void *param)
{
    (void) param;
    tud_init(BOARD_TUD_RHPORT);

    while (true)
    {
        tud_task();
    }
}

//--------------------------------------------------------------------+
// USB CDC Task
//--------------------------------------------------------------------+
static void cdc_task(void *param)
{
    while (true)
    {

        if (tud_cdc_connected())
        {
            if (tud_cdc_available())
            {
                parse_host_msg();
                tud_cdc_read_flush();
            }
        }
    }
}

//--------------------------------------------------------------------+
// Notification receiver task
//--------------------------------------------------------------------+
static void notif_recv_task(void *param)
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
                vTaskResume(msg_producer_handle);
            }else if (notificationvalue == 0U)
            {
                printf("Recieve notif %d \n", notificationvalue);
                printf("Suspending msg_producer task...\n");
                vTaskSuspend(msg_producer_handle);
            }else
            {
                printf("Unknown notif %d \n", notificationvalue);
            }
        }

    }
}

//--------------------------------------------------------------------+
// Message producer task
//--------------------------------------------------------------------+
static void message_producer_task(void *param)
{
    int32_t msg_produced = 0;
    int32_t msg_target = 10000;
    uint64_t encode_start = 0;
    uint64_t encode_end = 0;
    uint8_t msg_buf[256];
    pb_ostream_t stream;
    // Send OneOfMessage
    OneOfMessage msg1 = construct_183_bytes_msg();

    // Send AdcMessage
    // AdcMessage msg1 = construct_adc_msg();

    while (true)
    {
        // uint8_t msg_buf[64];
        if (msg_produced < msg_target)
        {
            if (msg_produced == 0)
            {
                printf("Start producing msgs in msg_producer_task...\n");
                start = time_us_64();
            }
            stream = pb_ostream_from_buffer(msg_buf, sizeof(msg_buf));
            encode_start = time_us_64();
            // Send OneOfMessage
            if (!pb_encode_ex(&stream, OneOfMessage_fields, &msg1, PB_ENCODE_DELIMITED))

            // Send AdcMessage
            // if (!pb_encode_ex(&stream, AdcMessage_fields, &msg1, PB_ENCODE_DELIMITED))
            {
                printf("Msg1 encode failed.\n");
            }else
            {
                encode_end = time_us_64();
                bytes_produced += stream.bytes_written;
                // Write to the egress msg buffer
                bytes_sent_to_msg_buf += xMessageBufferSend(egress_msg_buf_handle, msg_buf, stream.bytes_written, 0);
            }
            msg_produced++;

        }else
        {
            // Check if there is content in the message buffer, wait til it is empty
            // while((xMessageBufferIsEmpty(egress_msg_buf_handle) == pdFALSE)){};

            end = time_us_64();
            execution_time = end - start;
            printf("Attempt to transfer %d messages completed. \n", msg_target);
            printf("Each message is %d bytes. \n", stream.bytes_written);
            printf("start = %" PRIu64 "\n", start);
            printf("end = %" PRIu64 "\n", end);
            printf("total execution_time = %" PRIu64 " microseconds \n", execution_time);
            printf("pb_encode execution time = %" PRIu64 " microseconds \n", encode_end - encode_start);
            printf("bytes_produced = %d bytes \n", bytes_produced);
            printf("bytes_sent_to_msg_buf = %d bytes\n", bytes_sent_to_msg_buf);
            printf("bytes_recv_from_msg_buf = %d bytes\n", bytes_recv_from_msg_buf);
            printf("bytes_sent_to_host = %d bytes\n", bytes_sent_to_host);

            // Reset measurements variables
            bytes_produced = 0;
            bytes_sent_to_msg_buf = 0;
            bytes_recv_from_msg_buf = 0;
            bytes_sent_to_host = 0;
            msg_produced = 0;
            printf("Measurement variables have been reset.\n");

            // Suspend msg_producer task after msg target has been produced
            printf("Message_producer task suspended.\n");
            printf("\n");
            vTaskSuspend(NULL);

        }
        

    }
}

//--------------------------------------------------------------------+
// USB CDC print out task
//--------------------------------------------------------------------+
static void cdc_print_out_task(void *param)
{
    // bool led_state = 0;
    int32_t bytes_recv = 0;
    uint8_t msg_buf[256];
    while (true)
    {
        if (tud_cdc_connected())
        {
            if (!cdc_term_connected)
            {
                printf("cdc term connected!\n");
                // Send a task notification to core 1 to start producing messages
                xTaskNotify(notif_recv_handle, 1U, eSetValueWithOverwrite);
                printf("Send task notif with val 1U to notif_recv_handle to start production.\n");
                cdc_term_connected = 1;
            }
            // Fetch item from egress msg buf
            bytes_recv = xMessageBufferReceive(egress_msg_buf_handle, msg_buf, sizeof(msg_buf), 0);
            bytes_recv_from_msg_buf += bytes_recv;
            if (bytes_recv != 0)
            {
                // Write to cdc
                bytes_sent_to_host += tud_cdc_write(msg_buf, bytes_recv);
            }
        }else
        {
            if (cdc_term_connected)
            {
                printf("cdc term disconnected! \n");
                printf("\n");
                cdc_term_connected = 0;
            }
        }
            
    }
}


//--------------------------------------------------------------------+
// Host message buffer decode task
//--------------------------------------------------------------------+
static void host_msg_buf_decode_task(void *param)
{
    // Set up the decode callbacks
    // Decode OneOfMessage from host_msg_buf
    OneOfMessage msg = OneOfMessage_init_zero;
    // Set up the decode callbacks for contents in the oneof payload
    msg.cb_payload.funcs.decode = nanopb_msg_callback;

    // recv buffer, each message has maximum size of 256 bytes
    uint8_t recv_buf[256];
    int32_t recv_bytes = 0;
    while (true)
    {
        recv_bytes = xMessageBufferReceive(host_msg_buf_handle, recv_buf, sizeof(recv_buf), 0);
        if (recv_bytes > 0)
        {
            // Decode
            printf("Received %d bytes from msg buf.\n", recv_bytes);
            // Create a stream that reads from the msg buffer
            pb_istream_t stream = pb_istream_from_buffer(recv_buf, recv_bytes);

            bool status = pb_decode(&stream, OneOfMessage_fields, &msg); 
            if (status)
            {
                printf("Decode succeeded.\n");
                // Check the fields of message after decode and act accordingly
                if (msg.which_payload == OneOfMessage_msg2_tag)
                {
                    // Control LED_2 with led_on field
                    // gpio_put(LED_2, msg.payload.msg2.led_on);
                }
            }else
            {
                printf("Decode failed.\n");
            }
        }

    }
}

//--------------------------------------------------------------------+
// Blinky timer callback
//--------------------------------------------------------------------+
void led_blinky_cb(TimerHandle_t xTimer)
{
    (void) xTimer;
    static bool led_state = false;
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_state);
    led_state = 1 - led_state;
}