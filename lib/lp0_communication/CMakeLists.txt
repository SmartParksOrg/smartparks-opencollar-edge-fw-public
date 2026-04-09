zephyr_library()

zephyr_include_directories(.)
zephyr_library_sources(lp0_communication.c)

add_subdirectory(lp0_packetization)
