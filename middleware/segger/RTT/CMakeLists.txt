message("Building SEGGER RTT source files.")
add_library(segger_rtt INTERFACE)

target_sources(segger_rtt INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/Syscalls/SEGGER_RTT_Syscalls_GCC.c
        ${CMAKE_CURRENT_LIST_DIR}/Syscalls/SEGGER_RTT_Syscalls_IAR.c
        ${CMAKE_CURRENT_LIST_DIR}/Syscalls/SEGGER_RTT_Syscalls_KEIL.c
        ${CMAKE_CURRENT_LIST_DIR}/Syscalls/SEGGER_RTT_Syscalls_SES.c
        ${CMAKE_CURRENT_LIST_DIR}/RTT/SEGGER_RTT.c
        ${CMAKE_CURRENT_LIST_DIR}/RTT/SEGGER_RTT_printf.c
)

target_include_directories(segger_rtt INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/RTT
        ${CMAKE_CURRENT_LIST_DIR}/Config
)
