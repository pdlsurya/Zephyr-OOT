# SPDX-License-Identifier: Apache-2.0

add_subdirectory(sh1106_drv)
add_subdirectory_ifdef(CONFIG_NRF24_DRV nrf24_drv)
add_subdirectory_ifdef(CONFIG_SD_DRV SD_drv)
add_subdirectory_ifdef(CONFIG_TFT_DRV tft_drv)
