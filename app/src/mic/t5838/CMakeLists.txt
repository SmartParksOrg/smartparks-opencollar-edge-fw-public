# SPDX-License-Identifier: Apache-2.0

if(CONFIG_T5838)
  zephyr_include_directories(.)
  zephyr_library()

  # We don't exactly know why we need both of these, but it works.
  # TODO: CMake tree needs to be inspected.
  target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/t5838_interface.c)
  zephyr_library_sources(t5838_interface.c)
endif()
