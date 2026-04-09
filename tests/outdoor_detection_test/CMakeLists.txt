# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(outdoor_detection_test)

# Make a helper variable for the app directory
set(APP_DIR ../../app/main/src)

# Add source files with test code
file(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})

zephyr_include_directories(
  $ENV{ZEPHYR_BASE}/../project/app/src/outdoor_detection/common)
zephyr_library_sources(
  $ENV{ZEPHYR_BASE}/../project/app/src/outdoor_detection/common/common_utils.c)
