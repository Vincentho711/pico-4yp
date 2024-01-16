add_executable(freertos_task_notif
        task_notif.cpp
        )

# Include the FreeRTOSConfig.h into project
target_include_directories(freertos_task_notif PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        )

target_link_libraries(freertos_task_notif
        pico_stdlib              # for core functionality
        pico_cyw43_arch_none     # we need Wifi to access the GPIO, but we don't need anything else
        FreeRTOS-Kernel-Heap4    # Use heap_4.c for FreeRTOS memory management
        )

# If using the picoprobe, use it to relay stdio messages via uart, so enable uart and disable usb
# else enable usb output, disable uart output
pico_enable_stdio_usb(freertos_task_notif 0)
pico_enable_stdio_uart(freertos_task_notif 1)

# create map/bin/hex file etc.
pico_add_extra_outputs(freertos_task_notif)