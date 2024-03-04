add_executable(freertos_sw_timer
        sw_timer.cpp
        )

# Include the FreeRTOSConfig.h into project
target_include_directories(freertos_sw_timer PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        )

target_link_libraries(freertos_sw_timer
        pico_stdlib              # for core functionality
        pico_cyw43_arch_none     # we need Wifi to access the GPIO, but we don't need anything else
        FreeRTOS-Kernel-Heap4    # Use heap_4.c for FreeRTOS memory management
        # freertos_systemview      # For systemview functionality
        )

# If using the picoprobe, use it to relay stdio messages via uart, so enable uart and disable usb
# else enable usb output, disable uart output
pico_enable_stdio_usb(freertos_sw_timer 0)
pico_enable_stdio_uart(freertos_sw_timer 1)


# create map/bin/hex file etc.
pico_add_extra_outputs(freertos_sw_timer)