# Sensor drivers directory
This directory contains all the drivers for the sensors to be used with the data logger. Each driver is encapsulated into its own CMake library. These libraries are linked to the `sensor_drivers` CMake library.

## Implement a new driver library
To implement a new driver for a particular sensor, follow the following steps:
1. Create a new directory underneath the `sensors_drivers/` directory named after the sensor
2. Implement the relavent `.c` and `.h` files for the sensor, you can follow the example in the `ad7606b/` directory
3. Using the `ad7606b/CMakeLists.txt` as reference, implement a `CMakeLists.txt` for the sensor
4. Navigate to `sensor_drivers/inc/sensor_drivers.h`, include the header file for the new sensor
5. Navigate to `sensor_drivers/CMakeLists.txt`, add the directory of the driver with `add_subdirectory({your_driver_directory})`
6. Then also add the driver directory in `target_link_libraries(sensor_drivers INTERFACE ...)`
7. When you do `#include "sensor_drivers.h"`, you should be able to use the driver API calls