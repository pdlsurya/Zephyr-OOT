# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

get_filename_component(OOT_MODULE_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." REALPATH)
list(APPEND EXTRA_ZEPHYR_MODULES "${OOT_MODULE_DIR}")

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(peripheral_demo)

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE
  ${app_sources}
  )
