#ifndef AD7606B_H
#define AD7606B_H

#include <stdint.h>

#define ADC_SPI_CHANNEL      spi1
#define SPI1_SCLK_FREQ       16*1000*1000  // Frequency of SCLK for AD7606B
#define ADC_SPI_DATA_BITS    16            // The number of data bits for each SPI transfer


// Function prototypes
/**
 * @brief Initialise the AD7606B ADC
 */
void ad7606b_init(void);

/**
 * @brief Reset the AD7606B ADC
 */
void ad7606b_reset(void);

/**
 * @brief Start conversion on ADC, a conversion takes 4 micro-seconds with oversampling ratio of 4
 */
void ad7606b_convert(void);

/**
 * @brief Sample from the ADC, which includes the conversion and read out
 *
 * @param dest_buf The pointer to the starting address of the destination buffer
 * @param elementsTransferred The number of existing elements in the destination buffer, the correct starting address to populate from will be calculated
 * @param active_adc_chan The number of active adc channels
 */
void ad7606b_sample(int32_t* dest_buf, uint8_t* elementsTransferred, uint8_t active_adc_chan);


#endif /* AD7606B_h */

