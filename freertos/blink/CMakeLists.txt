add_executable(freertos_blink
        blink.cpp
        )

# Include the FreeRTOSConfig.h into project
target_include_directories(freertos_blink PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        )

target_link_libraries(freertos_blink
        pico_stdlib              # for core functionality
        pico_cyw43_arch_none     # we need Wifi to access the GPIO, but we don't need anything else
        FreeRTOS-Kernel-Heap4    # Use heap_4.c for FreeRTOS memory management
        )

# create map/bin/hex file etc.
pico_add_extra_outputs(freertos_blink)