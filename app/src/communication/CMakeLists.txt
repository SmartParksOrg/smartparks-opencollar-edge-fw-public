zephyr_include_directories(.)

target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/communication.c)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lr_messaging)
