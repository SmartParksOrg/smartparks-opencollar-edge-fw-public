# SPDX-License-Identifier: Apache-2.0

if(CONFIG_VM3011)
  zephyr_include_directories(.)
  target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/vm3011_interface.c)
  target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/vm3011.c)

endif()
