if (NOT FREERTOS_KERNEL_PATH AND NOT DEFINED ENV{FREERTOS_KERNEL_PATH})
    message("FREERTOS_KERNEL_PATH is not defined. Please set it in root CMakeLists.txt first.")
else()
    message("Found FreeRTOS library, building device_main with FreeRTOS.")
    # Initialise all other FreeRTOS Kernel external lib & dependencies
    include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

    # Check whether the sensor_drivers directory exists
    set(SENSOR_DRIVERS_DIR ${CMAKE_CURRENT_LIST_DIR}/../sensor_drivers)
    if (NOT EXISTS ${SENSOR_DRIVERS_DIR})
      message(FATAL_ERROR "SENSOR_DRIVERS_DIR does not exist : ${SENSOR_DRIVERS_DIR}.")
    endif()

    add_executable(device_main)
    target_sources(device_main PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/main.c
        # ${CMAKE_CURRENT_LIST_DIR}/usb_descriptors.c
        )

    target_include_directories(device_main PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/inc
        )

    target_link_libraries(device_main
        pico_stdlib                             # for core functionality
        pico_cyw43_arch_none                    # Use vanilla cyw43_arch, no wifi+lwip
        pico_unique_id                          # For a function in usb_descriptors.cpp, get unique id
        pico_multicore                          # Use both cores for execution
        # tinyusb_device                          # For using the pico version of tinyusb
        # tinyusb_board                           # For using pico version of tinyusb
        tinyusb_device_freertos                 # For using freertos version of tinyusb
        tinyusb_board_freertos                  # For using freertos version of tinyusb
        FreeRTOS-Kernel-Heap4                   # Use heap_4.c for FreeRTOS memory management
        segger_rtt                              # Use SEGGER RTT for fast debugging
        sensor_drivers                          # A library which contains all individual sensor drivers
        hardware_spi
        )

    # Disable both stdio output with usb and uart
    pico_enable_stdio_usb(device_main 0)
    pico_enable_stdio_uart(device_main 1)

    # create map/bin/hex file etc.
    pico_add_extra_outputs(device_main)
endif()
