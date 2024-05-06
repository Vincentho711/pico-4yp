#include "ad7606b.h"
#include "board_config.h"
#include <pico/cyw43_arch.h>
#include <pico/stdlib.h>
#include <hardware/gpio.h>
#include <hardware/spi.h>

void ad7606b_init(void)
{
  // Set SCLK frequency
  spi_init(ADC_SPI_CHANNEL, SPI1_SCLK_FREQ);
  gpio_set_function(SPI1_RX_PIN, GPIO_FUNC_SPI);
  gpio_set_function(SPI1_CS_PIN, GPIO_FUNC_SPI);
  gpio_set_function(SPI1_SCK_PIN, GPIO_FUNC_SPI);
  spi_set_format(ADC_SPI_CHANNEL, ADC_SPI_DATA_BITS, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
  gpio_set_dir(SPI1_CS_PIN, GPIO_OUT);
  // CS is active low, so initialise it to a high state
  gpio_set_dir(SPI1_CS_PIN, 1);
  // Initialise the rest of the ADC pins
  gpio_init(ADC_BUSY_PIN);
  gpio_set_dir(ADC_BUSY_PIN, GPIO_IN);
  gpio_init(SPI1_TX_PIN);
  gpio_set_dir(SPI1_TX_PIN, GPIO_IN);
  gpio_init(ADC_CONVST_PIN);
  gpio_set_dir(ADC_CONVST_PIN, GPIO_OUT);
  gpio_put(ADC_CONVST_PIN, 0);
  gpio_init(ADC_RESET_PIN);
  gpio_set_dir(ADC_RESET_PIN, GPIO_OUT);
  gpio_put(ADC_RESET_PIN, 0);
}

void ad7606b_reset(void)
{
  gpio_put(ADC_RESET_PIN, 1);
  sleep_us(1);
  gpio_put(ADC_RESET_PIN, 0);
  sleep_us(0);
}

void ad7606b_convert(void)
{
  gpio_put(ADC_CONVST_PIN, 1);
  // Sleep for 8*4 = 32ns
  __asm volatile ("nop\n");
  __asm volatile ("nop\n");
  __asm volatile ("nop\n");
  __asm volatile ("nop\n");
  gpio_put(ADC_CONVST_PIN, 0);
  // Sleep for 8*6 - 48ns
  __asm volatile ("nop\n");
  __asm volatile ("nop\n");
  __asm volatile ("nop\n");
  __asm volatile ("nop\n");
  __asm volatile ("nop\n");
  __asm volatile ("nop\n");
}

void ad7606b_sample(int32_t* dest_buf, uint8_t* elementsTransferred, uint8_t active_adc_chan)
{
  uint16_t adc_data [8];
  ad7606b_convert();
  // Wait if the ADC is busy
  while (gpio_get(ADC_BUSY_PIN));
  // Read 8 16-bits in a blocking manner, this reads all 8 channels even though
  // some channels are not active
  spi_read16_blocking(ADC_SPI_CHANNEL, 0, adc_data, 8);

  // Convert uint16_t to store in int32_t destination buffer, only convert active adc channels
  for (int i=0; i<active_adc_chan; i++)
  {
    dest_buf[*elementsTransferred] = (int32_t) adc_data[i];
    (*elementsTransferred)++;
  }
}
