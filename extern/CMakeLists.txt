# Add sudmodules for cmake to discover them and compile with source code
set(GIT_DIR_LOOKUP_POLICY ALLOW_LOOKING_ABOVE_CMAKE_SOURCE_DIR)
add_subdirectory(etl)
# Exclude FreeRTOS-Kernel as it is automatically picked up by FreeRTOS_Kernel_import.cmake within the FreeRTOS lib
# add_subdirectory(FreeRTOS-Kernel)
add_subdirectory(nanopb)