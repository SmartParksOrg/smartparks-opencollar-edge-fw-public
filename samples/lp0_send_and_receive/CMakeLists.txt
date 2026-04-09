# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr HINTS $ENV{ZEPHYR_BASE})
project(lp0_packetization)

zephyr_compile_options(-fdiagnostics-color=always)

zephyr_library_sources(src/main.c)

# include the "samples common" folder from the library
add_subdirectory(${ZEPHYR_BASE}/../irnas/SWDR001-Zephyr/samples/common
                 src/common)
