# Data Acquisition System (DAS)
![das host interface](https://github.com/Vincentho711/pico-4yp/blob/main/figures/das_host_interface.png?raw=true)
A data acquisition system based on the Raspberry Pi Pico W platform for collecting data from various sensors, built for the civil engineering division.

## Description
The project features both the source code for the [DAS device](device_src) and the [host interface](host_src/python_host_scripts/) written in Python, as well as [sensor drivers](sensor_drivers) and the [sensors_manager](device_src/lib/sensor_manager/) library to manage function selection with static dispatch. It demonstrates the ability for the host interface to connect to the DAS device via USB, and configure it to sample the ADC periodically at an arbitrary frequency below 25 kHz using nanopb messages, with data streamed back to host and displayed on the host CLI.

Furthermore, it contains many examples within the [examples](examples) directory, with the most interesting ones located within the [examples/freertos](examples/freertos/) directory. This directory is organised to facilitate the exploration and testing of specific features under the FreeRTOS architecture, with examples building upon one another to form the foundation of the DAS. Each example within the freertos directory is designed for independent compilation and uploading onto the Raspberry Pi Pico W.

## Getting Started

### Dependencies

* pico-sdk

### Installing

1. Clone [pico-sdk](https://github.com/raspberrypi/pico-sdk), version 1.5.1 should be used to avoid compatability issues, with SHA-1 hash : 6a7db34ff63345a7badec79ebea3aaef1712f374

2. Change the `PICO_SDK_PATH` in [CMakeLists.txt](CMakeLists.txt) to point to the directory which contains the pico-sdk

3. To enable TinyUSB to work with FreeRTOS, 3 files of from the pico-sdk and TinyUSB library need to be modified, see comment in [device_src/inc/tsub_config.h](device_src/inc/tusb_config.h) for instructions

### Executing program
There are multiple ways to build the project with CMake. The most straightforward way involves using the `CMAKE Tools` extension in VSCode. After executing CMake, the `Cortex-Debug` extension can be used to select which project firmware to upload onto the Pico W, and start a debug session once a **`launch.json`** has been defined.
The [J-Link EDU Mini](https://www.segger.com/products/debug-probes/j-link/models/j-link-edu-mini/) is the recommended debug probe, which features unlimited breakpoints and allows [RTT](https://www.segger.com/products/debug-probes/j-link/technology/about-real-time-transfer/) to be used through the [RTT Viewer](https://www.segger.com/products/debug-probes/j-link/tools/rtt-viewer/).
