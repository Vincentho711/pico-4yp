#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include "pico/stdio.h"
#include "sensor_drivers.h"

// Enum to represent different types of sensors
enum sensorFamily
{
  MBA500_LOAD_CELL_SENSOR
};

// Struct to describe a sensor
struct Sensor
{
  int sensor_id;
  char name [64];
  enum sensorFamily family;
  bool useADC;
  int sampling_time_us; // Used for calculating the upper limit of sampling frequency for digital sensors
};

// A generic struct which stores information about the number of connected sensors
// and an array which contains them
struct connectedSensors
{
  int num_of_con;
  struct Sensor* sensor_array[8];
};

// A pointer which points to the sample function of a particular sensor
typedef void (*sensor_sample_func)(int32_t* dest_buf, uint8_t* elementsTransferred);

// Function prototypes

/**
 * @brief Sample from the ADC
 *
 * @param dest_buf A pointer to the starting address of the destination buffer
 * @param elementsTransferred The number of existing elements in the destination buffer, the correct starting address to populate from will be calculated
 * @param active_adc_chan The number of active adc channels
 */
void adc_sample_func(int32_t* dest_buf, uint8_t* elementsTransferred, uint8_t active_adc_chan);

/**
 * @brief Get the sample function of a sensor based on its family name
 *
 * @param sensor The sensor object with field faily
 * @return A function pointer of the sample function for that sensor
 */
sensor_sample_func get_sensor_sample_func(const struct Sensor* sensor);


#endif /* SENSOR_MANAGER_h */
