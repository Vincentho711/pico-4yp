/**
 * @file main.c
 * @brief The c entry file for the data logger device
 */
#include <stdio.h>
#include <inttypes.h>
#include "ad7606b.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "sensor_manager.h"
#include "pico/time.h"
#include <SEGGER_RTT.h>
#include "FreeRTOS.h"
#include "task.h"
#include "tusb.h"
#include <pb_encode.h>
#include <pb_decode.h>
#include <pb_common.h>
#include <main.pb.h>

bool led_on = 0;
uint8_t num_of_adc_chan = 0;


int main()
{
  SEGGER_RTT_Init();
  stdio_init_all();

  if (cyw43_arch_init())
  {
    SEGGER_RTT_printf(0, "Wi-Fi init failed.\n");
    return -1;
  }else
  {
    SEGGER_RTT_printf(0, "Wi-Fi init succeeded.\n");
  }
  ad7606b_init();
  ad7606b_reset();

  // Initialise TinyUSB stack
  tusb_init();

  // Destination buffer used to store sensor values obtained
  int32_t dest_buf[16] = {0};
  uint8_t elementsTransferred = 0;

  // Initialise all possible sensors here, the family tag is used to map to sample functions
  struct Sensor mba500_load_cell_torque = {0, "MBA500_load_cell_torque", MBA500_LOAD_CELL_SENSOR, true};
  struct Sensor mba500_load_cell_thrust = {1, "MBA500_load_cell_thrust", MBA500_LOAD_CELL_SENSOR, true};

  // Initialise a connectedSensors struct
  struct connectedSensors connected_sensors;

  // Initialise an array which stores sample function pointers
  sensor_sample_func sensor_sample_func_array[8];

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
  HostToDeviceMessage msg = HostToDeviceMessage_init_zero;

  while (true)
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
}
