# Add sudmodules for cmake to discover them and compile with source code
set(GIT_DIR_LOOKUP_POLICY ALLOW_LOOKING_ABOVE_CMAKE_SOURCE_DIR)
add_subdirectory(etl)
add_subdirectory(FreeRTOS-Kernel)