#ifndef AD7606B_H
#define AD7606B_H

#include <stdint.h>

#define ADC_SPI_CHANNEL      spi1
#define SPI1_SCLK_FREQ       16*1000*1000  // Frequency of SCLK for AD7606B
#define ADC_SPI_DATA_BITS    16            // The number of data bits for each SPI transfer


// Function prototypes
void ad7606b_init(void);
void ad7606b_reset(void);
void ad7606b_convert(void);
void ad7606b_sample(uint16_t dest_buf[8]);


#endif /* AD7606B_h */

