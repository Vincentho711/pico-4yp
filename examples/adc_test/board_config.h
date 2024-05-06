#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

// Define pins for SPI1
#define SPI1_SCK_PIN    14 // AD7606B's SPI 1 CLK pin
#define SPI1_RX_PIN     12 // AD7606B's SPI 1 RX pin
#define SPI1_TX_PIN     11 // AD7606B's ADC FRST pin, indicate first data output
#define SPI1_CS_PIN     13 // AD7606B's CS pin

// Define pins for AD7606B ADC
#define ADC_BUSY_PIN    10 // AD7606's BUSY output pin
#define ADC_CONVST_PIN  22 // AD7606's conversion start input pin
#define ADC_RESET_PIN   31 // AD7606's ADC reset pin, active high


/* // Define pins for I2C
#define I2C_SCL_PIN     A5
#define I2C_SDA_PIN     A4

// Define other pin connections
#define SENSOR_1_PIN    2
#define SENSOR_2_PIN    3
// Add more as needed...
*/
#endif /* BOARD_CONFIG_H */
