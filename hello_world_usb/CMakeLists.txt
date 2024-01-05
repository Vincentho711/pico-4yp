if (TARGET tinyusb_device)
    add_executable(hello_world_usb
            picow_hello_world_usb.cpp
            )

    # pull in common dependencies
    target_link_libraries(hello_world_usb
        pico_stdlib
        pico_cyw43_arch_none     # we need Wifi to access the GPIO, but we don't need anything else
    )

    # If using the picoprobe, use it to relay UART messages, so enable uart and disable usb
    # else enable usb output, disable uart output
    pico_enable_stdio_usb(hello_world_usb 0)
    pico_enable_stdio_uart(hello_world_usb 1)

    # create map/bin/hex/uf2 file etc.
    pico_add_extra_outputs(hello_world_usb)

elseif(PICO_ON_DEVICE)
    message(WARNING "not building hello_usb because TinyUSB submodule is not initialized in the SDK")
endif()
