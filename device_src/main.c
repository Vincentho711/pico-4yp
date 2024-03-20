/**
 * @file main.c
 * @brief The c entry file for the data logger device
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

  int16_t adc_data[8] = {0};

  while (true)
  {
    led_on = !led_on;
    // Blink the onboard LED
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_on);
    // Sample from the ADC, blocking
    ad7606b_sample((uint16_t*)adc_data);

    SEGGER_RTT_printf(0,"Time = %" PRIu64 "\n", get_absolute_time());
    for (int i = 0; i < 8; i++)
    {
      SEGGER_RTT_printf(0, "%d = %d\n", i, adc_data[i]);
    }
    SEGGER_RTT_printf(0, "\n");
    sleep_ms(1000);
  }
  return 0;
}
