# For using nanopb
set(CMAKE_MODULE_PATH ${NANOPB_SRC_ROOT_FOLDER}/extra)
find_package(Nanopb REQUIRED)

# Generate nanopb source and header files in c
nanopb_generate_cpp(PROTO_SRCS PROTO_HDRS RELPATH proto proto/unionproto.proto)

# Generate the nanopb classes for using Python as host
set(PROTO_PY ${CMAKE_CURRENT_LIST_DIR}/proto/unionprot_pb2.py)
add_custom_command(
        OUTPUT ${PROTO_PY}
        COMMAND protoc -I=${CMAKE_CURRENT_LIST_DIR}/proto 
        --python_out=${CMAKE_CURRENT_LIST_DIR}/python_host_scripts 
        ${CMAKE_CURRENT_LIST_DIR}/proto/unionproto.proto
        )
# Invoke custom command for generating nanopb python files
add_custom_target(proto_py ALL DEPENDS ${PROTO_PY})       

add_executable(freertos_usb_nanopb_command_interface_no_lwip
        ${PROTO_SRCS}                     # Include the generated nanopb source files
        ${PROTO_HDR}                      # Include the generated nanopb header files
        )

target_sources(freertos_usb_nanopb_command_interface_no_lwip PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/usb_nanopb_command_interface_no_lwip.c
        ${CMAKE_CURRENT_LIST_DIR}/usb_descriptors.c
        )

# Include the FreeRTOSConfig.h into project
target_include_directories(freertos_usb_nanopb_command_interface_no_lwip PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}       # Including the generated header files for nanopb
        ${NANOPB_INCLUDE_DIRS}            # Including the common header files for nanopb
        )

target_link_libraries(freertos_usb_nanopb_command_interface_no_lwip
        pico_stdlib                             # for core functionality
        pico_cyw43_arch_none                    # Use vanilla cyw43_arch, no wifi+lwip
        pico_unique_id                          # For a function in usb_descriptors.cpp, get unique id
        # tinyusb_device                          # For using the pico version of tinyusb
        # tinyusb_board                           # For using pico version of tinyusb
        tinyusb_device_freertos                 # For using freertos version of tinyusb
        tinyusb_board_freertos                  # For using freertos version of tinyusb
        FreeRTOS-Kernel-Heap4                   # Use heap_4.c for FreeRTOS memory management
        )

# If using the picoprobe, use it to relay stdio messages via uart, so enable uart and disable usb
# else enable usb output, disable uart output
# For this example, we are using the usb output, so disable uart use for stdio to avoid conflict
pico_enable_stdio_usb(freertos_usb_nanopb_command_interface_no_lwip 0)
pico_enable_stdio_uart(freertos_usb_nanopb_command_interface_no_lwip 1)

# create map/bin/hex file etc.
pico_add_extra_outputs(freertos_usb_nanopb_command_interface_no_lwip)