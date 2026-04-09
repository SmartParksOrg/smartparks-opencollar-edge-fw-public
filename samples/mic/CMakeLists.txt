# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)

set(BOARD_ROOT ${CMAKE_CURRENT_LIST_DIR}/../..)
# selects the board that the application’s
# build will use for the default configuration
set(BOARD rangeredge_nrf52840)

find_package(Zephyr HINTS $ENV{ZEPHYR_BASE})
project(mic_sample)

zephyr_compile_options(-fdiagnostics-color=always)

target_sources(app PRIVATE src/main.c)
