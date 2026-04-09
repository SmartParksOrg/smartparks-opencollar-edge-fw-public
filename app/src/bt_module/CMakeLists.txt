zephyr_include_directories(.)
target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/bt_interface.c)

add_subdirectory(bt_con)
add_subdirectory(bt_nus)
add_subdirectory(bt_adv)
add_subdirectory(bt_scan)
add_subdirectory_ifdef(CONFIG_BT_CMDQ bt_cmdq)
