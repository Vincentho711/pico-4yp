# Set the DEVICE_SRC_PATH
# The path needs to be exposed to drivers as it needs to use board_config.h
# for the pins defines
set(DEVICE_SRC_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../device_src)

# Check if the DEVICE_SRC_PATH exists
if(EXISTS ${DEVICE_SRC_PATH})
    message("DEVICE_SRC_PATH exists: ${DEVICE_SRC_PATH}")
    set(BOARD_CONFIG_INC_DIR ${DEVICE_SRC_PATH}/inc)
    # Check if the inc dir exists
    if (NOT EXISTS ${BOARD_CONFIG_INC_DIR})
        message(FATAL_ERROR "BOARD_CONFIG_INC_DIR does not exist or it is not set correctly.")
    endif()
    # Check if the board_config.h exists
    if (NOT EXISTS "${BOARD_CONFIG_INC_DIR}/board_config.h")
        message(FATAL_ERROR "board_config.h does not exist in ${BOARD_CONFIG_INC_DIR}.")
    endif()
    message("board_config.h found in ${BOARD_CONFIG_INC_DIR}, continue to build individual sensor drivers.")

    # Add all the individual sensor driver libraries
    add_subdirectory(ad7606b) # AD7606B ADC

    # Create a "sensor_drivers" library which encompasses all sensor drivers
    add_library(sensor_drivers INTERFACE)

    target_include_directories(sensor_drivers INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/inc
        )

    target_link_libraries(sensor_drivers INTERFACE
        ad7606b
        )
    
else()
    message(FATAL_ERROR "DEVICE_SRC_PATH does not exist or it is not set correctly.")
endif()
