if (TARGET tinyusb_device)
    add_executable(etl_test
            etl_test.cpp
            )

    # pull in common dependencies
    target_link_libraries(etl_test
        PRIVATE pico_stdlib
        PRIVATE pico_cyw43_arch_none     # we need Wifi to access the GPIO, but we don't need anything else
        PRIVATE etl::etl
    )

    # enable usb output, disable uart output
    pico_enable_stdio_usb(etl_test 1)
    pico_enable_stdio_uart(etl_test 0)

    # create map/bin/hex/uf2 file etc.
    pico_add_extra_outputs(etl_test)

elseif(PICO_ON_DEVICE)
    message(WARNING "not building hello_usb because TinyUSB submodule is not initialized in the SDK")
endif()