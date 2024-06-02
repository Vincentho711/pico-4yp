/**
 * @file main.c
 * @brief The c entry file for the data logger device
 */
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/time.h"
#include "pico/multicore.h"

#include <stdio.h>
#include <inttypes.h>

#include "ad7606b.h"
#include "sensor_manager.h"

#include <SEGGER_RTT.h>

#include "FreeRTOS.h"
#include "task.h"
#include "message_buffer.h"

#include "tusb.h"

#include <pb_encode.h>
#include <pb_decode.h>
#include <pb_common.h>
#include <main.pb.h>

// Default task stack size
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
// #define EGRESS_MSG_BUF_SIZE  256*10

// Egress stream buffer size in bytes
#define EGRESS_STREAM_BUF_SIZE 256*10

// USB device task handle
TaskHandle_t usbd_handle_c0 = NULL;

// CDC egress task handle
TaskHandle_t cdc_egress_handle_c1 = NULL;

// CDC ingress task handle
TaskHandle_t cdc_ingress_handle_c0 = NULL;

// Periodic sampler task handle
TaskHandle_t periodic_sampler_handle_c1 = NULL;

// Egress msg buffer handle
// MessageBufferHandle_t egress_msg_buf_handle = NULL;

// Egress stream buffer handle
StreamBufferHandle_t egress_stream_buf_handle = NULL;

// Repeating timer for periodic sampler
repeating_timer_t periodic_sampler_timer;

// Core 0 tasks
static void usb_device_task_c0(void *param);
// static void cdc_task_c0(void *param);
static void cdc_ingress_task_c0(void *param);

// Core 1 tasks
static void cdc_egress_task_c1(void *param);
static void periodic_sampler_task_c1(void *param);

// Local callback functions
static bool execute_sampler(struct connectedSensors connected_sensors, bool operating_mode, uint8_t adc_chan_no, int32_t* dest_buf, uint8_t* elemenetsTransferred);
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

bool led_on = 0;
uint8_t num_of_adc_chan = 0;

// For keeping cdc connection status
bool cdc_term_connected = 0;

// For keeping track of whether a periodic sampler is active
bool active_periodic_sampler = 0;

// For keeping track of the element sent in periodic_sampler_cd(), used for testing
uint32_t sampler_counter = 0;

// Initialise an array which stores sample function pointers
sensor_sample_func sensor_sample_func_array[8];

// Initialise a connectedSensors struct
struct connectedSensors connected_sensors;

int main()
{
  SEGGER_RTT_Init();

  // Destination buffer used to store sensor values obtained
  // int32_t dest_buf[16] = {0};
  // uint8_t elementsTransferred = 0;

  // Initialise all possible sensors here, the family tag is used to map to sample functions
  struct Sensor mba500_load_cell_torque = {0, "MBA500_load_cell_torque", MBA500_LOAD_CELL_SENSOR, true, 0};
  struct Sensor mba500_load_cell_thrust = {1, "MBA500_load_cell_thrust", MBA500_LOAD_CELL_SENSOR, true, 0};

  if (cyw43_arch_init())
  {
    SEGGER_RTT_printf(0, "Wi-Fi init failed.\n");
    printf("Wi-Fi init failed.\n");
  }else
  {
    SEGGER_RTT_printf(0, "Wi-Fi init suceeded.\n");
    printf("Wi-Fi init suceeded.\n");
  }

  // Reset the ADC
  ad7606b_init();
  ad7606b_reset();

  // Initialise TinyUSB stack
  tusb_init();

  // Create a task for tinyusb device stack task
  assert(xTaskCreate(usb_device_task_c0, "usbd_c0", USBD_STACK_SIZE, NULL, configMAX_PRIORITIES-1, &(usbd_handle_c0)) == pdPASS);
  assert(usbd_handle_c0 != NULL);
  // Configure the usb device task affinity mask, it can only run on core 0
  UBaseType_t uxCoreAffinityMask = ((1<<0));
  vTaskCoreAffinitySet(usbd_handle_c0, uxCoreAffinityMask);

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

  // Create a stream buffer to store egress messages
  egress_stream_buf_handle = xStreamBufferCreate(EGRESS_STREAM_BUF_SIZE, 64);
  assert (egress_stream_buf_handle != NULL);


  /*
    The sensor discovery mechanism has not been implemented. But we assume that we have
    done that and registers the plugged-in sensors instances to the connected_sensors struct
  */

  // Register the sensors with the connctedSensors struct
  connected_sensors.num_of_con = 2;
  connected_sensors.sensor_array[0] = &mba500_load_cell_torque;
  connected_sensors.sensor_array[1] = &mba500_load_cell_thrust;

  // Check whether the ADC needs to be used, and find the sample function for sensors
  for (int i=0;i<connected_sensors.num_of_con;i++)
  {
    num_of_adc_chan += connected_sensors.sensor_array[i]->useADC;
    sensor_sample_func_array[i] = get_sensor_sample_func(connected_sensors.sensor_array[i]);
  }

  // Initialise HostToDeviceMessage structure
  // HostToDeviceMessage msg = HostToDeviceMessage_init_zero;
  //

  // Start scheduler
  vTaskStartScheduler();
  while (true) {};

  /* while (true)
  {
    led_on = !led_on;
    // Blink the onboard LED
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_on);
    // Reset elementsTransferred
    elementsTransferred = 0;

    // Check whether ADC needs to be sampled first
    if (num_of_adc_chan > 0)
    {
      adc_sample_func(dest_buf, &elementsTransferred, num_of_adc_chan);
    }

    // Sample other sensors which doesn't use the ADC
    for (int i = 0; i < connected_sensors.num_of_con; i++)
    {
      if (sensor_sample_func_array[i] != NULL)
      {
        sensor_sample_func_array[i](dest_buf, &elementsTransferred);
      }
    }

    // Print the destination buffer
    SEGGER_RTT_printf(0,"Time = %" PRIu64 "\n", get_absolute_time());
    SEGGER_RTT_printf(0, "elementsTransferred = %" PRIu8 "\n", elementsTransferred);
    for (int i = 0; i < 16; i++)
    {
      SEGGER_RTT_printf(0, "%d = %d\n", i, dest_buf[i]);
    }
    SEGGER_RTT_printf(0, "\n");
    sleep_ms(10);

    tud_task();
  }
  return 0;
  */
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
  }
  else if (field->tag == HostToDeviceMessage_execute_one_off_sampler_msg_tag)
  {
    SEGGER_RTT_printf(0, "Got execute_one_off_sampler_msg.\n");
    ExecuteOneOffSamplerMessage *msg = (ExecuteOneOffSamplerMessage*)field->pData;
  }
  else
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
// CDC egress task (Core 1)
//--------------------------------------------------------------------+
static void cdc_egress_task_c1(void *param)
{
  // bool led_state = 0;
  int32_t bytes_recv = 0;
  uint8_t msg_buf[256];
  while (true)
  {
    bytes_recv = xStreamBufferReceive(egress_stream_buf_handle, msg_buf, sizeof(msg_buf), 0);
    if (bytes_recv != 0)
    {
      tud_cdc_write(msg_buf, bytes_recv);
    }
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

      }else if (msg.which_payload == HostToDeviceMessage_execute_one_off_sampler_msg_tag)
      {
        // Buffer to store encoded data
        uint8_t msg_buf[32];
        // Stream object
        pb_ostream_t stream;

        // Sample on Core 0
        int32_t dest_buf[8] = {0};
        uint8_t elementsTransferred = 0;
        SEGGER_RTT_printf(0, "Executing one-off sampler.\n");
        execute_sampler(connected_sensors, false, num_of_adc_chan, dest_buf, &elementsTransferred);

        // Construct OneOffSamplerDataMessage
        DeviceToHostMessage msg = DeviceToHostMessage_init_zero;
        msg.payload.one_off_sampler_data_msg.sensor_val_0 = dest_buf[0];
        msg.payload.one_off_sampler_data_msg.sensor_val_1 = dest_buf[1];
        msg.payload.one_off_sampler_data_msg.sensor_val_2 = dest_buf[2];
        msg.payload.one_off_sampler_data_msg.sensor_val_3 = dest_buf[3];
        msg.payload.one_off_sampler_data_msg.sensor_val_4 = dest_buf[4];
        msg.payload.one_off_sampler_data_msg.sensor_val_5 = dest_buf[5];
        msg.payload.one_off_sampler_data_msg.sensor_val_6 = dest_buf[6];
        msg.payload.one_off_sampler_data_msg.sensor_val_7 = dest_buf[7];
        msg.which_payload = DeviceToHostMessage_one_off_sampler_data_msg_tag;

        stream = pb_ostream_from_buffer(msg_buf, sizeof(msg_buf));

        // Encode and send one off sampler data message back to host
        if (!pb_encode_ex(&stream, DeviceToHostMessage_fields, &msg, PB_ENCODE_DELIMITED))
        {
          SEGGER_RTT_printf(0, "ERROR : DeviceToHostMessage encode failed.\n");
          printf("ERROR : DeviceToHostMessage encode failed.\n");
        }
        uint32_t bytes_written = tud_cdc_write(msg_buf, stream.bytes_written);

        // When the number of bytes transmitted is small, they will hang around
        // in the TX FIFO and accumulate before a critical mass is reached for
        // Tinyusb to send all of them with a bulk transfer, we want to transmit
        // instantly, so force send with tud_cdc_write_flush()
        bytes_written = tud_cdc_write_flush();
        assert(bytes_written = stream.bytes_written);
        SEGGER_RTT_printf(0, "one_off_sampler_data_msg sent. Msg length = %" PRIu32 "\n", bytes_written);
      }else
      {
        SEGGER_RTT_printf(0, "ERROR : Nanopb message decode failure.\n");
      }
    }
    // Suspend task after a host-to-device message has been parsed to free up execution time
    // for cdc_egress_task_c0
    vTaskSuspend(NULL);
  }
}

// The underlying sampler to execute for both one-off & periodic sampler
static bool execute_sampler(struct connectedSensors connected_sensors, bool operating_mode, uint8_t adc_chan_no, int32_t* dest_buf, uint8_t* elementsTransferred)
{
  // uint8_t elementsTransferred = 0;

  // Check whether the ADC needs to be sampled, and find the sample function for sensors
  if (adc_chan_no > 0)
  {
    // Sample from ADC
    adc_sample_func(dest_buf, elementsTransferred, num_of_adc_chan);
  }

  // Sample other sensors
  for (int i = 0; i < connected_sensors.num_of_con; i++)
  {
    if (sensor_sample_func_array[i] != NULL)
    {
      sensor_sample_func_array[i](dest_buf, elementsTransferred);
    }
  }

  return true;

}
// Callback executed when the repeating periodic sampler timer expires
static bool periodic_sampler_cb(struct repeating_timer *t)
{

  // SEGGER_RTT_printf(0,"Time = %" PRIu64 "\n", get_absolute_time());
  // SEGGER_RTT_printf(0, "Executed periodic_sampler_cb()\n");
  int32_t dest_buf[8] = {0};
  uint8_t elementsTransferred = 0;

  // Execute the sampler
  execute_sampler(connected_sensors, false, num_of_adc_chan, dest_buf, &elementsTransferred);

  // Put content into egress stream buffer such that it can be transmitted to host
  uint32_t bytes_written = xStreamBufferSendFromISR(egress_stream_buf_handle, &dest_buf, sizeof(dest_buf), NULL);

  // Check that every byte is written into stream buffer
  assert(bytes_written == sizeof(dest_buf));
  // At high sampling frequency, executing the following printing code in an interrupt is not ideal as they will take time
  // Better to comment them out
  /* if (bytes_written != sizeof(dest_buf))
  {
    SEGGER_RTT_printf(0, "ERROR : bytes dropped due to full egress stream buf. TX FIFO avail = %" PRIu32 "bytes.\n", tud_cdc_write_available());
  }else
  {
    SEGGER_RTT_printf(0, "Bytes written = %" PRIu32 "bytes.\n", bytes_written);
  }

  // Print the destination buffer
  SEGGER_RTT_printf(0,"Time = %" PRIu64 "\n", get_absolute_time());
  SEGGER_RTT_printf(0, "elementsTransferred = %" PRIu8 "\n", elementsTransferred);
  for (int i = 0; i < 8; i++)
  {
    SEGGER_RTT_printf(0, "%d = %d\n", i, dest_buf[i]);
  }
  SEGGER_RTT_printf(0, "\n");
  */
  return true;
    /* uint16_t msg_buf[24];
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
    // Check that the entire msg has been written into MessageBuffer
    assert(bytes_written == sizeof(msg_buf));
    // SEGGER_RTT_printf(0, "Wrote %d bytes to device in periodic_sampler_cb().\n", bytes_written);
    // SEGGER_RTT_printf(0, "Periodic sampling executed, end timestamp = %" PRIu64 "\n", time_us_64());
    // printf("Wrote %d bytes to device in periodic_sampler_cb().\n", bytes_written);
    // assert(xMessageBufferSendFromISR(egress_msg_buf_handle, msg_buf, sizeof(msg_buf), 0) != 0); */
}
