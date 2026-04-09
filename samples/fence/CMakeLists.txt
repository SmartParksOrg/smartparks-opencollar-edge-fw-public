# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

set(BOARD_ROOT ${CMAKE_CURRENT_LIST_DIR}/../..)
# selects the board that the application’s
# build will use for the default configuration
set(BOARD rangeredge_nrf52840)

find_package(Zephyr HINTS $ENV{ZEPHYR_BASE})
project(blinky)

zephyr_compile_options(-fdiagnostics-color=always)

target_sources(app PRIVATE src/main.c)

# add include directories from application
add_subdirectory(../../app/src/sensors/fence_port/fence)
