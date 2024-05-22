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

    # For using nanopb
    set(CMAKE_MODULE_PATH ${NANOPB_SRC_ROOT_FOLDER}/extra)
    find_package(Nanopb REQUIRED)

    # Set the proto source directory
    set(MAIN_HOST_PROTO_SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/../proto)
    # Set the entry proto file
    set(MAIN_HOST_PROTO_SRC_FILE ${MAIN_HOST_PROTO_SRC_DIR}/main.proto)

    message("MAIN_HOST_PROTO_SRC_DIR: " ${MAIN_HOST_PROTO_SRC_DIR})
    message("MAIN_HOST_PROTO_SRC_FILE: " ${MAIN_HOST_PROTO_SRC_FILE})

    # Generate nanopb source and header files in c
    nanopb_generate_cpp(PROTO_SRCS PROTO_HDRS RELPATH proto ${MAIN_HOST_PROTO_SRC_FILE})

    # Set the destination file for the host nanopb classes in Python
    set(MAIN_HOST_PROTO_PY ${CMAKE_CURRENT_LIST_DIR}/../proto/main_pb2.py)
    # Set the output directory for the Python nanopb classes
    set(MAIN_HOST_PROTO_OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/../host_src/python_host_scripts)

    message("MAIN_HOST_PROTO_PY: " ${MAIN_HOST_PROTO_PY})
    message("MAIN_HOST_PROTO_OUTPUT_DIR: " ${MAIN_HOST_PROTO_OUTPUT_DIR})

    # Generate the host nanopb Python file
    add_custom_command(
        OUTPUT ${MAIN_HOST_PROTO_PY}
        COMMAND protoc -I=${MAIN_HOST_PROTO_SRC_DIR}
        --python_out=${MAIN_HOST_PROTO_OUTPUT_DIR}
        ${MAIN_HOST_PROTO_SRC_FILE}
        )

    # Invoke custom command for generating nanopb python files
    add_custom_target(main_proto_py ALL DEPENDS ${MAIN_HOST_PROTO_PY})

    # Add the executable and include the generated nanopb source and header files in C
    add_executable(device_main
        ${PROTO_SRCS}             # Include the generated nanopb source files
        ${PROTO_HDR}              # Include the generated nanopb header files
        )

    # Add the lib directory to build internal libraries
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib)

    target_sources(device_main PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/main.c
        ${CMAKE_CURRENT_LIST_DIR}/usb_descriptors.c
        )

    target_include_directories(device_main PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/inc
        ${CMAKE_CURRENT_BINARY_DIR}         # Including the generated header files for nanopb
        ${NANOPB_INCLUDE_DIRS}              # Inclduing the common header files for nanopb
        )

    target_link_libraries(device_main
        pico_stdlib                             # for core functionality
        pico_cyw43_arch_none                    # Use vanilla cyw43_arch, no wifi+lwip
        pico_unique_id                          # For a function in usb_descriptors.cpp, get unique id
        pico_multicore                          # Use both cores for execution
        tinyusb_device                          # For using the pico version of tinyusb
        # tinyusb_board                           # For using pico version of tinyusb
        tinyusb_device_freertos                 # For using freertos version of tinyusb
        tinyusb_board_freertos                  # For using freertos version of tinyusb
        FreeRTOS-Kernel-Heap4                   # Use heap_4.c for FreeRTOS memory management
        segger_rtt                              # Use SEGGER RTT for fast debugging
        sensor_manager                          # A library which contains the sensor manager and all sensor drivers
        )

    # Disable both stdio output with usb and uart
    pico_enable_stdio_usb(device_main 0)
    pico_enable_stdio_uart(device_main 1)

    # create map/bin/hex file etc.
    pico_add_extra_outputs(device_main)
endif()
