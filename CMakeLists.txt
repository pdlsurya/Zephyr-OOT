# SPDX-License-Identifier: Apache-2.0

add_subdirectory(drivers)
add_subdirectory(subsys)

zephyr_include_directories(
subsys/sh1106_gfx
subsys/sdFat32
subsys/nrf24_ble                     
drivers/sh1106_drv
drivers/nrf24_drv)
