if (NOT EXISTS "${BOARD_CONFIG_INC_DIR}/board_config.h")
    message(FATAL_ERROR "board_config.h does not exist in ${BOARD_CONFIG_INC_DIR}.")
endif()
message("Building AD7606B source files.")
add_library(ad7606b INTERFACE)

target_sources(ad7606b INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/ad7606b.c
    )

target_include_directories(ad7606b INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
    ${BOARD_CONFIG_INC_DIR}     # Include board_config.h so pins can be used in driver
    )

target_link_libraries(ad7606b
    INTERFACE
        pico_stdlib
        pico_cyw43_arch_none
        hardware_spi
    )
