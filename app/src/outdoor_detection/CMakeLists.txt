zephyr_include_directories(.)
zephyr_include_directories(./profiles/pangolin)
zephyr_include_directories(./common)

target_sources(
  app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/profiles/pangolin/pangolin_logic.c)
target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/common/common_utils.c)
target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/outdoor_detection.c)
