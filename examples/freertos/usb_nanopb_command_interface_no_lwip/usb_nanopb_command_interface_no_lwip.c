/**
 * @file usb_device_cdc_dual_ports_no_lwip.c
 * @author Vincent Ho (mch79@cam.ac.uk)
 * @brief 
 * @version 0.1
 * @date 2024-01-23
 * 
 * @copyright NA
 * 
 */

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include <stdio.h>

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
#include "unionproto.pb.h"

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

// Host msg buffer size in bytes
#define HOST_MSG_BUF_SIZE  256*4

// Extra LEDs to show status
#define LED_1       28
#define LED_2       17

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

// Host msg buffer handler
MessageBufferHandle_t host_msg_buf_handle = NULL;

// For testing
int32_t received_msg = 0;

void led_blinky_cb(TimerHandle_t xTimer);
static void usb_device_task(void *param);
static void cdc_task(void *param);
static void host_msg_buf_decode_task(void *param);

static void parse_host_msg(void);
static void decode_oneof_message_streaming(void);

bool nanopb_msg_callback(pb_istream_t *stream, const pb_field_t *field, void **arg);
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
    gpio_init(LED_1);
    gpio_set_dir(LED_1, GPIO_OUT);

    // Initialise LED_2
    gpio_init(LED_2);
    gpio_set_dir(LED_2, GPIO_OUT);

    // Turn off both LED 1 & 2 to start
    gpio_put(LED_1, 0);
    gpio_put(LED_2, 0);

    // Start with LED ON
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
  
    // Create a sw timer for blinky
    blinky_tm = xTimerCreate(NULL, pdMS_TO_TICKS(BLINK_NOT_MOUNTED), pdTRUE, NULL, led_blinky_cb);
    assert (blinky_tm != NULL);
  
    // Create a task for tinyusb device stack task
    assert(xTaskCreate(usb_device_task, "usbd", USBD_STACK_SIZE, NULL, configMAX_PRIORITIES-1, NULL) == pdPASS);

    // Create a task for tinyusb cdc task
    assert(xTaskCreate(cdc_task, "cdc", CDC_STACK_SIZE, NULL, configMAX_PRIORITIES-2, NULL) == pdPASS);

    // Create a message buffer to store host msg for decoding
    host_msg_buf_handle = xMessageBufferCreate(HOST_MSG_BUF_SIZE);
    assert (host_msg_buf_handle != NULL);

    // Create a task to decode host messages from message buffer
    assert(xTaskCreate(host_msg_buf_decode_task, "host_msg_decode", HOST_MSG_DECODE_STACK_SIZE, NULL, configMAX_PRIORITIES-3, NULL) == pdPASS);

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

    // Increment received_msg count
    received_msg++;

    if (field->tag == OneOfMessage_msg1_tag)
    {
        printf("Got MsgType1.\n");
        MsgType1 *msg1 = (MsgType1*)field->pData;
        msg1->value.funcs.decode = nanopb_cb_print_int32;
        msg1->value.arg = "value : %d \n";
    }
    else if (field->tag == OneOfMessage_msg2_tag)
    {
        printf("Got MsgType2.\n");
        MsgType2 *msg2 = (MsgType2*)field->pData;
        msg2->value.funcs.decode = nanopb_cb_print_int32;
        msg2->value.arg = "value : %d\n";
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
            gpio_put(LED_1, 1);
            if (tud_cdc_available())
            {
                parse_host_msg();
                tud_cdc_read_flush();
            }
        }else
        {
            gpio_put(LED_1, 0);
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
                    gpio_put(LED_2, msg.payload.msg2.led_on);
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
    // printf("LED: %d\n", led_state);
    printf("Received msg: %d\n", received_msg);
    led_state = 1 - led_state;
}