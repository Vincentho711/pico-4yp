add_executable(adc_dma)
target_sources(adc_dma PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/adc_dma.c
    )

target_include_directories(adc_dma PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    )

target_link_libraries(adc_dma
    pico_stdlib                             # for core functionality
    pico_cyw43_arch_none                    # Use vanilla cyw43_arch, no wifi+lwip
    # pico_unique_id                          # For a function in usb_descriptors.cpp, get unique id
    # pico_multicore                          # Use both cores for execution
    # tinyusb_device                          # For using the pico version of tinyusb
    # tinyusb_board                           # For using pico version of tinyusb
    # tinyusb_device_freertos                 # For using freertos version of tinyusb
    # tinyusb_board_freertos                  # For using freertos version of tinyusb
    # FreeRTOS-Kernel-Heap4                   # Use heap_4.c for FreeRTOS memory management
    segger_rtt                              # Use SEGGER RTT for fast debugging
    sensor_drivers                          # A library which contains all individual sensor drivers
    )

# Disable both stdio output with usb and uart
pico_enable_stdio_usb(adc_dma 0)
pico_enable_stdio_uart(adc_dma 1)

# create map/bin/hex file etc.
pico_add_extra_outputs(adc_dma)