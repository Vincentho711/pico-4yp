cmake_minimum_required(VERSION 3.12)

# Set board
set(PICO_BOARD "pico_w")
# Set sdk path
set(PICO_SDK_PATH "/Users/vincentho/pico/pico-sdk")
# Set Wifi credentials
set(WIFI_SSID "pico_4yp")
set(WIFI_PASSWORD "pwd")

# Pull in SDK (must be before project), located in cmake dir
# set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
include(cmake/pico_sdk_import.cmake)
include(cmake/pico_extras_import_optional.cmake)

project(pico_4yp C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

if (PICO_SDK_VERSION_STRING VERSION_LESS "1.3.0")
    message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.3.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()



# Initialize the SDK
pico_sdk_init()

# Add extern lib
add_subdirectory(extern)

# Add docs generation through doxygen
add_subdirectory(docs)

add_compile_options(-Wall
        -Wno-format          # int != int32_t as far as the compiler is concerned because gcc has int32_t as long int
        -Wno-unused-function # we have some for the docs that aren't called
        )
        
# Add blink example
add_subdirectory(blink)
# Add hello world usb example
add_subdirectory(hello_world_usb)
# Add etl_test example
add_subdirectory(etl_test)

if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wno-maybe-uninitialized)
endif()

