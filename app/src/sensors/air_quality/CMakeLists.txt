zephyr_include_directories(.)
target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/air_quality.c)

add_subdirectory(bme69x_sensor)
add_subdirectory(bmv080_sensor)
