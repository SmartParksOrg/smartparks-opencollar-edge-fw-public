zephyr_include_directories(.)
target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lr11xx_almanac.c)
target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/almanac.c)
