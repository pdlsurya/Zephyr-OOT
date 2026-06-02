# ZephyrRTOS OOT Module

Out-of-tree drivers, subsystems, and application samples for Zephyr.

This repository is used as an external Zephyr module. It collects board-specific
drivers, reusable subsystems, and a handful of sample applications that exercise
those pieces together.

## What is in this repo

- `drivers/`
  - `sh1106_drv`: SH1106 OLED driver glue used by the demo apps
  - `nrf24_drv`: NRF24L01 driver used by the BLE-over-NRF24 demos
- `subsys/`
  - `sh1106_gfx`: simple graphics helpers for the OLED demos
  - `nrf24_ble`: BLE advertising helpers on top of the NRF24 radio
  - `sdFat32`: FAT32 filesystem layer adapted from the standalone `sdFat32`
    project, wired to Zephyr's SDHC/SDMMC stack
- `app_samples/`
  - `esp32c6_blinky`: minimal LED blink sample for the ESP32-C6 DevKitC
  - `z_esp32-wroom32`: ESP32-C6 demo combining OLED, NRF24, and SD card access
  - `central_multilink`: ESP32 demo for multilink BLE central experiments
  - `ble_peripheral`: BLE peripheral sample with the local OOT pieces
  - `z_nrf52_dongle`: nRF52 dongle sample
  - `z_stm32f401ccu6`: STM32F401 sample

## Zephyr version

The repo is currently being exercised against Zephyr `4.3.0`.

The ESP32-family samples also need the Espressif blobs fetched in the Zephyr
workspace:

```sh
cd ~/zephyrproject
west blobs fetch hal_espressif
```

## Using this repo as a module

Each sample in `app_samples/` adds this repository through
`EXTRA_ZEPHYR_MODULES`, so the simplest workflow is to build from inside a
sample directory.

For a generic application outside this repo, point `EXTRA_ZEPHYR_MODULES` at the
repository root:

```sh
export EXTRA_ZEPHYR_MODULES=~/EmbeddedProjects/ZephyrRTOS
```

or append it from CMake:

```cmake
get_filename_component(OOT_MODULE_DIR "/absolute/path/to/ZephyrRTOS" REALPATH)
list(APPEND EXTRA_ZEPHYR_MODULES "${OOT_MODULE_DIR}")
```

## Sample highlights

### `app_samples/esp32c6_blinky`

Small sanity-check sample for `esp32c6_devkitc/esp32c6/hpcore`. It only needs a
`led0` alias supplied by the sample overlay and is a good first build when
checking the workspace, toolchain, and flash flow.

### `app_samples/z_esp32-wroom32`

Multi-peripheral ESP32-C6 demo that pulls in:

- `CONFIG_SD_FAT32`
- `CONFIG_OLED_SH1106`
- `CONFIG_SH1106_GFX`
- `CONFIG_NRF24_DRV`
- `CONFIG_NRF24_BLE`

The sample overlay enables:

- SH1106 on I2C
- SD card over SPI through Zephyr's SD subsystem
- NRF24 over SPI
- `led0` alias for the onboard LED

### `app_samples/central_multilink`

ESP32 BLE central demo. On Zephyr `4.3.0` it builds with:

```sh
west build -b esp32_devkitc/esp32/procpu
```

### `app_samples/z_stm32f401ccu6`

Carries the STM32-side OLED, SD, and NRF24 wiring used during bring-up. This
sample still assumes a board definition outside upstream Zephyr, so it may need
local board work before it is buildable in a fresh workspace.

## Build examples

From inside the sample directories:

```sh
source ~/zephyrproject/zephyr/zephyr-env.sh
west build -b esp32c6_devkitc/esp32c6/hpcore
```

or use the provided helper scripts, for example:

```sh
cd app_samples/esp32c6_blinky
./build.sh
```

## Notes on `sdFat32`

This repo does not carry a separate SD card block driver anymore. The local
`sdFat32` port keeps only the FAT32 implementation and uses Zephyr's existing
SDHC/SDMMC path underneath.

That means the relevant sample configuration should enable Zephyr's SD support,
for example:

```conf
CONFIG_SDHC=y
CONFIG_SDMMC_STACK=y
CONFIG_SDMMC_SUBSYS=y
CONFIG_SD_FAT32=y
```
