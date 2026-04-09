zephyr_include_directories(.)
target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/flash_interface.c
                           ${CMAKE_CURRENT_SOURCE_DIR}/flash_ext_partitions.c)
