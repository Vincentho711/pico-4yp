add_executable(freertos_usb_device_cdc_dual_ports_no_lwip)

target_sources(freertos_usb_device_cdc_dual_ports_no_lwip PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/usb_device_cdc_dual_ports_no_lwip.c
        ${CMAKE_CURRENT_LIST_DIR}/usb_descriptors.c
        )

# Include the FreeRTOSConfig.h into project
target_include_directories(freertos_usb_device_cdc_dual_ports_no_lwip PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        )

target_link_libraries(freertos_usb_device_cdc_dual_ports_no_lwip
        pico_stdlib                             # for core functionality
        pico_cyw43_arch_none                    # Use vanilla cyw43_arch, no wifi+lwip
        pico_unique_id                          # For a function in usb_descriptors.cpp, get unique id
        # tinyusb_device                          # For using the pico version of tinyusb
        # tinyusb_board                           # For using pico version of tinyusb
        tinyusb_device_freertos                 # For using freertos version of tinyusb
        tinyusb_board_freertos                  # For using freertos version of tinyusb
        FreeRTOS-Kernel-Heap4                   # Use heap_4.c for FreeRTOS memory management
        # freertos_systemview                     # For systemview functionality
        )

# If using the picoprobe, use it to relay stdio messages via uart, so enable uart and disable usb
# else enable usb output, disable uart output
# For this example, we are using the usb output, so disable uart use for stdio to avoid conflict
pico_enable_stdio_usb(freertos_usb_device_cdc_dual_ports_no_lwip 0)
pico_enable_stdio_uart(freertos_usb_device_cdc_dual_ports_no_lwip 1)

# create map/bin/hex file etc.
pico_add_extra_outputs(freertos_usb_device_cdc_dual_ports_no_lwip)