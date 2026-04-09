zephyr_include_directories(.)
target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lorawan.c)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/almanac)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/gnss)
add_subdirectory_ifdef(CONFIG_LR_WIFI_SCAN
                       ${CMAKE_CURRENT_SOURCE_DIR}/wifi_scan)
add_subdirectory_ifdef(CONFIG_LR_S_BAND ${CMAKE_CURRENT_SOURCE_DIR}/lr_s_band)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/vhf)
