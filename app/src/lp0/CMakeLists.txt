zephyr_include_directories(.)
zephyr_library_sources(lp0.c)

add_subdirectory(lp0_offload)
add_subdirectory(lp0_common)
