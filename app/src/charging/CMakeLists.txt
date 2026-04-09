zephyr_include_directories(.)
target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/charging_interface.c)
target_sources(
  app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/charging_voltage_adjustment.c)
