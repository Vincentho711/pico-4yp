if (NOT FREERTOS_KERNEL_PATH AND NOT DEFINED ENV{FREERTOS_KERNEL_PATH})
    message("Skip building Pico W FreeRTOS examples as FREERTOS_KERNEL_PATH is not defined. Please set it in root CMakeLists.txt first.")
else()
    message("Found FreeRTOS library, building FreeRTOS examples...")
    # Initialise all other FreeRTOS Kernel external lib & dependencies
    include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

    add_subdirectory(blink)
    add_subdirectory(sw_timer)
    add_subdirectory(task_notif)
    add_subdirectory(usb_device_hid_composite_no_lwip)
    add_subdirectory(usb_device_cdc_dual_ports_no_lwip)
    add_subdirectory(usb_nanopb_command_interface_no_lwip)
endif()