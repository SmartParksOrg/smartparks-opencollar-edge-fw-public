zephyr_include_directories(.)
target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/wifi_scan.c)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/wifi_scan_printers)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/wifi_scan_data)
