# Raspberry Pi Pico W Egress Interface with TinyUSB (Multi-core)

This C project delves into the performance evaluation of a data egress interface on the Raspberry Pi Pico W. The Pico W is configured as a CDC (Communication Device Class) USB device, establishing communication with a host device (such as a laptop running a pyserial script). The USB stack employed for this interaction is TinyUSB, while nanopb is utilised for the encoding and decoding of messages.

The primary objective of this project is to quantify the throughput of the data egress interface when operating on FreeRTOS with TinyUSB. Additionally, the project seeks to scrutinise the impact of using nanopb for real-time data encoding and decoding. The assessment involves the configuration of nanopb messages with varying sizes, transmitting them from the device to the host, and measuring both the time taken for transmission and the delivery rate.

It's essential to note that the generation and transmission of nanopb messages are distributed across two tasks, each allocated to a specific core. This multi-core approach enhances the efficiency of the egress interface, contributing to its overall performance.

## Description
When the Pico W establishes a connection with a host, and a serial terminal is initiated on the host (or the `python_host_scripts/tinyusb_multi_core_egress_interface.py` script is executed), the Pico W takes on the responsibility of generating data. Subsequently, it encodes this data into nanopb messages and transmits them to the host via the TinyUSB interface. The configuration of this test scenario is visually represented in the figure below.

![tinyusb_multi_core_egress_interface_test_setup](./figures/tinyusb_multi_core_egress_interface.png)

Core 0 of the RP2040 takes on the role of managing communication with the host. It is responsible for retrieving nanopb messages from a FreeRTOS message buffer as soon as they become available for transmission to the host. In contrast, Core 1 actively generates and encodes data into nanopb messages. Furthermore, it is responsible for depositing these messages onto the FreeRTOS message buffer, ensuring a seamless flow of data through the system.

To systematically evaluate the system and identify potential bottlenecks, a series of tests can be conducted by modifying the messages transmitted. This involves varying the size of messages and their types. The specific structure of these messages is outlined in the `proto/egress_multi_core.proto` file.

## Technology utilised
### Hardware features
- [ ] Hardware timer interrupts

### Libraries
- [x] pico-sdk
- [x] FreeRTOS
- [ ] lwIP
- [x] TinyUSB
- [x] nanopb

### FreeRTOS features
- [x] Software timers
- [ ] Queues
- [ ] Semaphore/Mutexes
- [x] Task notifications
- [x] Message buffers
- [ ] Stream buffers
- [x] SMP

### Nanopb features
#### Encode
- [x] Encode
- [ ] Encode strings
- [ ] Encode OneOf messages

#### Decode
- [ ] Decode
- [ ] Decode strings
- [ ] Decode OneOf messages

## Getting Started

### Dependencies
The pico-sdk doesn't currently support using the FreeRTOS version of TinyUSB, hence custom modifications of the pico-sdk and TinyUSB repo is required for the CMAKE compile definitions to be passed down in order to extract the correct header files. The modifications below are required for using TinyUSB on FreeRTOS:
* Modify `CMakeLists.txt` of pico-sdk, with changes shown in https://github.com/raspberrypi/pico-sdk/pull/1438 (the next version of pico-sdk might well have this commit merged into main)
* Modify `family.cmake` in `pico-sdk/lib/tinyusb/hw/bsp/rp2040/family.cmake`. Comment out the line `CFG_TUSB_OS=OPT_OS_PICO` in `target_compile_definition(tinyusb_common_base ...)`

### Building and flashing the device code
Navigate to the root of this project `pico-4yp`, create a `build` directory using `mkdir build`. Then navigate into the `build` directory and run
```console
cmake ..
```
Navigate into this project by
```console
cd freertos/usb_device_tinyusb_multi_core_egress_interface_no_lwip/
```
and run
```console
make -j4
```
This will create the `elf` file which can be uploaded onto the Raspberry Pico W. To flash this file onto it directly with openocd, 
```console
sudo openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg -c "adapter speed 5000" -c "program freertos_usb_device_tinyusb_multi_core_egress_interface_no_lwip.elf verify reset exit"
```
Once it is uploaded successfully, we reset and should be able to see a TinyUSB device appearing with
```console
ioreg -p IOUSB
```

### Executing host program
The host will be communicating with the USB CDC device through a Python script located in `python_host_script/tinyusb_multi_core_egress_interface.py`. A serial connection is established and data is sent from the device to host. Don't forget to change the `port_name` in the Python script before executing it. It doesn't matter what the `baud_rate` is as USB will attempt to send/receive data as quick as possible. The script just outputs all the data received from the device without decoding, so streams of bytes will be displayed. The script will maintain the serial connection once the data has been transferred, users will have to exit the script manually through <kbd>Ctrl</kbd> + <kbd>c</kbd>. 

## Investigation Results

### Results table
| TX FIFO buffer size (bytes) | Egress msg buffer size (bytes) | Message producer msg_buf size (bytes) | Message Type | No. of bytes per message (bytes) | No. of messages produced by source | Bytes produced by source (bytes) | Bytes sent to egress msg buffer (bytes) | Bytes received from egress msg buffer (bytes) | Bytes sent to host (bytes) | Total execution time (s) | pb_encode() execution time (micro-s) | Delivery rate* (%) | Source generation rate (Mbps) | Actual Throughput*(Mbps) |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1024 | 256*10 = 2560 | 256 | OneOfMessage | 26 | 10000 | 260000 | 260000 | 260000 | 260000 | 1.274212 | 109.3 | 100 | 1.6324 | 1.6324 |
| 1024 | 256*10 = 2560 | 64 | OneOfMessage | 26 | 10000 | 260000 | 260000 | 260000 | 260000 | 1.276863 | 101 | 100 | 1.6290 | 1.6290 |
| 1024 | 256*2 = 512 | 256 | AdcMessage | 30 | 10000 | 300000 | 300000 | 300000 | 300000 | 1.450959 | 115.6 | 100 | 1.6541 | 1.6541 |
| 1024 | 256*10 = 2560 | 256 | AdcMessage | 46 | 10000 | 460000 | 460000 | 460000 | 460000 | 1.576065 | 142 | 100 | 2.3349 | 2.3349 |
| 1024 | 256*10 = 2560 | 256 | AdcMessage | 54 | 10000 | 540000 | 540000 | 540000 | 540000 | 1.488700 | 130.17 | 100 | 2.9019 | 2.9019 |
| 1024 | 256*10 = 2560 | 256 | OneOfMessage | 49 | 10000 | 490000 | 490000 | 490000 | 490000 | 1.403351 | 110.85 | 100 | 2.7933 | 2.7933 |
| 1024 | 256*10 = 2560 | 256 | OneOfMessage | 98 | 10000 | 980000 | 980000 | 980000 | 931252 | 1.153268 | 102.22 | 95.03 | 6.7981 | 6.7981 |
| 2048 | 256*10 = 2560 | 256 | OneOfMessage | 98 | 10000 | 980000 | 980000 | 980000 | 871288 | 1.060891 |  | 88.90 | 7.3900 | 6.5702 |
| 1024 | 256*10 = 2560 | 256 | OneOfMessage | 183 | 10000 | 1830000 | 1830000 | 1830000 | 994713 | 1.234290 | 104.89 | 54.36 | 11.8611 | 6.4472 |
| 2048 | 256*10 = 2560 | 256 | OneOfMessage | 183 | 10000 | 1830000 | 1830000 | 1830000 | 962455 | 1.152113 |  | 52.59 | 12.7071 | 6.6831 |
| 4096 | 256*10 = 2560 | 256 | OneOfMessage | 183 | 10000 | 1830000 | 1830000 | 1830000 | 926541 | 1.125548 |  | 50.63 | 13.0070 | 6.5955 |

*Delivery rate is calculated using $\text{Delivery rate} = \frac{\text{Bytes sent to host}}{\text{Bytes produced by source}}$.

*Actual throughput is calculated by only accounting all the bytes that have been received by the host, hence $\text{Actual throughput (Mbps)} = \frac{\text{Bytes sent to host * 8}}{\text{Execution time}}$.

### Conclusions
- The `tud_cdc_write()` function operates in a blocking manner, meaning that if called while the TX FIFO buffer is full, bytes will not be transmitted, resulting in dropped data.
- When the no. of bytes per nanopb message is small, the tinyusb stack can keep up with the source generation rate easily. Therefore, the TX FIFO buffer is not being utilised and the source generation rate matches with the actual throughput. There are no bottlenecks in this case.
- When the no. of bytes per message gets too big (at 98 and 183 bytes per message), messages start being dropped due to the TX FIFO being 100% utilised and the blocking nature of `tud_cdc_write()`.
- Increasing the TX FIFO buffer size does not improve delivery rate and actual throughput for large message sizes, as tinyUSB library imposes a maximum limit.
- The time spent on encoding proto messages with `pb_encode()` remains constant, independent of message size or type. Encoding data into a nanopb message takes approximately 100 microseconds for one message.
- It's noteworthy that the maximum throughput of TinyUSB operating on FreeRTOS with the setup defined in `tusb_config.h` appears to be around 6.5Mbps. This provides a reference point for the system's overall performance.