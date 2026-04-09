zephyr_include_directories(.)
file(GLOB app_sources ${CMAKE_CURRENT_SOURCE_DIR}/*.c)
target_sources(app PRIVATE ${app_sources})
