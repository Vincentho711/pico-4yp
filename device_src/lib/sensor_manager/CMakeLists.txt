# Create a sensor manager library which includes all the sensors drivers
# and orchestrate them
add_library(sensor_manager INTERFACE)

target_sources(ad7606b INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/sensor_manager.c
  )

target_include_directories(sensor_manager INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}
  )

target_link_libraries(sensor_manager INTERFACE
  sensor_drivers
  )
