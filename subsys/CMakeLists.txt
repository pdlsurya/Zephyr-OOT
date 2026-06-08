# SPDX-License-Identifier: Apache-2.0

# Subdirectories
add_subdirectory_ifdef(CONFIG_SH1106_GFX sh1106_gfx)
add_subdirectory_ifdef(CONFIG_NRF24_BLE nrf24_ble)
add_subdirectory_ifdef(CONFIG_SD_FAT32 sdFat32)
add_subdirectory_ifdef(CONFIG_TFT_GFX tft_gfx)
