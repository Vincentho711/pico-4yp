#include "sensor_manager.h"
#include "ad7606b.h"

void adc_sample_func(int32_t* dest_buf, uint8_t* elementsTransferred, uint8_t active_adc_chan)
{
  ad7606b_sample(dest_buf, elementsTransferred, active_adc_chan);
}

sensor_sample_func get_sensor_sample_func(const struct Sensor* sensor)
{
  enum sensorFamily family = sensor->family;
  switch (family)
  {
    case MBA500_LOAD_CELL_SENSOR:
      return NULL; // Return NULL as it uses the ADC to sample
      break;
    default:
      return ((void *) 0);
  }

}
