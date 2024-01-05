if (NOT FREERTOS_KERNEL_PATH AND NOT DEFINED ENV{FREERTOS_KERNEL_PATH})
    message("Skip building Pico W FreeRTOS examples as FREERTOS_KERNEL_PATH is not defined. Please set it in root CMakeLists.txt first.")
else()
    message("Building FreeRTOS examples...")
    # Initialise all other FreeRTOS Kernel external lib & dependencies
    include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)
    add_subdirectory(blink)
endif()