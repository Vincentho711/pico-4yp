#include "ad7606b.h"
#include "board_config.h"
#include <pico/cyw43_arch.h>
#include <pico/stdlib.h>
#include <hardware/gpio.h>
#include <hardware/spi.h>


/**
 * @brief Initialise the AD7606B ADC
 */
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

/**
 * @brief Reset the AD7606B ADC
 */
void ad7606b_reset(void)
{
  gpio_put(ADC_RESET_PIN, 1);
  sleep_us(1);
  gpio_put(ADC_RESET_PIN, 0);
  sleep_us(0);
}

/**
 * @brief Start conversion on ADC, a conversion takes 4 micro-seconds with oversampling ratio of 4
 */
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

void ad7606b_sample(uint16_t dest_buf[8])
{
  ad7606b_convert();
  // Wait if the ADC is busy
  while (gpio_get(ADC_BUSY_PIN));
  // Read 16 bytes in a blocking manner
  // Only sample first 8 bytes which contains data bytes for ADC channel 1 & 2
  spi_read16_blocking(ADC_SPI_CHANNEL, 0, dest_buf, 8);
}
