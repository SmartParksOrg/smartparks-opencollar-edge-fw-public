#
# Copyright (c) 2018 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
#
cmake_minimum_required(VERSION 3.13.1)

# Use the correct partition map based on the build type
if("${OVERLAY_CONFIG}" MATCHES "debug.conf")
  set(PM_STATIC_YML_FILE ${CMAKE_CURRENT_SOURCE_DIR}/pm_static_debug.yml)
else()
  set(PM_STATIC_YML_FILE ${CMAKE_CURRENT_SOURCE_DIR}/pm_static.yml)
endif()

#list(APPEND DTS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

# Modifications to the build system
# need to be applied before calling find_package
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(opencollar_fw)

zephyr_compile_options(-fdiagnostics-color=always)

target_sources(app PRIVATE src/main.c)
# NORDIC SDK APP END

zephyr_library_include_directories(.)

# Application directories
add_subdirectory(src/sensors)
add_subdirectory(src/communication)
add_subdirectory(src/battery)
add_subdirectory(src/charging)
add_subdirectory(src/gps_ublox)
add_subdirectory(src/settings)
add_subdirectory(src/threads)
add_subdirectory_ifdef(CONFIG_FLASH_MODULE src/flash)
add_subdirectory(src/bt_module)
add_subdirectory_ifdef(CONFIG_ENABLE_MIC src/mic)
add_subdirectory_ifdef(CONFIG_PROVISIONING_MODE src/operation_test)
add_subdirectory_ifdef(CONFIG_SATELLITE src/satellite)
add_subdirectory(src/led)
add_subdirectory(src/reed)
add_subdirectory(src/lorawan)
add_subdirectory_ifdef(CONFIG_OUTDOOR_DETECTION src/outdoor_detection)
add_subdirectory(src/lp0)
