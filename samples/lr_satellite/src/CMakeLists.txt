zephyr_include_directories(.)

add_subdirectory(transmit_rule)

target_sources(app PRIVATE main.c)
