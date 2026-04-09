# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr HINTS $ENV{ZEPHYR_BASE})
project(sdfs_uart_sample)

zephyr_library_sources(src/main.c)
