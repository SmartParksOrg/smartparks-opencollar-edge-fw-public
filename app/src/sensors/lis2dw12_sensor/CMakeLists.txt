zephyr_include_directories(.)
target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lis2dw12_sensor.c)
target_sources_ifdef(CONFIG_IRNAS_LIS2DW12_FIFO app PRIVATE
                     ${CMAKE_CURRENT_SOURCE_DIR}/lis2dw12_sensor_fifo.c)
