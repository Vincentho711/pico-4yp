/**
 * @file adc_test.c
 * @brief A test to sample from the AD7606b
 */


#include <stdio.h>
#include <inttypes.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "sensor_drivers.h"
#include "pico/time.h"
#include <SEGGER_RTT.h>

bool led_on = 0;
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

  int32_t dest_buf[16] = {0};
  uint8_t bytesTransferred = 0;
  uint8_t active_adc_chan = 2;

  while (true)
  {
    led_on = !led_on;
    // Blink the onboard LED
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_on);
    // Reset the bytesTransferred
    bytesTransferred = 0;
    // Sample from the ADC, blocking
    ad7606b_sample(dest_buf, &bytesTransferred, active_adc_chan);

    SEGGER_RTT_printf(0,"Time = %" PRIu64 "\n", get_absolute_time());
    for (int i = 0; i < 16; i++)
    {
      SEGGER_RTT_printf(0, "%d = %d\n", i, dest_buf[i]);
    }
    SEGGER_RTT_printf(0, "\n");
    sleep_ms(1000);
  }
  return 0;
}
