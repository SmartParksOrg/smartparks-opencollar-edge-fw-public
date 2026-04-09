zephyr_include_directories(.)
zephyr_include_directories(./external_lib)

zephyr_library()
zephyr_library_sources(sdfs_uart_handler.c)
zephyr_library_sources(external_lib/fprot.c)
