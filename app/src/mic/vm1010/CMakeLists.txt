# SPDX-License-Identifier: Apache-2.0

if(CONFIG_VM1010)
  zephyr_include_directories(.)
  zephyr_library()

  zephyr_library_sources(vm1010.c)
endif()
