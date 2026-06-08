# Zephyr-OOT Module

Out-of-tree board definitions, drivers, subsystems, and application samples for
Zephyr.

This repository is used as an external Zephyr module. It collects board-specific
board definitions, drivers, reusable subsystems, and a handful of sample
applications that exercise those pieces together.

Public headers are exported under `include/zephyr_oot/`, for example:

- `#include <zephyr_oot/drivers/tft_drv.h>`
- `#include <zephyr_oot/subsys/tft_gfx.h>`
- `#include <zephyr_oot/subsys/sh1106_gfx.h>`

## What is in this repo

- `drivers/`
  - `sh1106_drv`: SH1106 OLED driver glue used by the demo apps
  - `nrf24_drv`: NRF24L01 driver used by the BLE-over-NRF24 demos
  - `SD_drv`: SPI-mode SD card driver used by the FAT32 layer
  - `tft_drv`: Zephyr-style SPI TFT display driver with controller-specific init and pixel-write helpers
- `subsys/`
  - `sh1106_gfx`: simple graphics helpers for the OLED demos
  - `nrf24_ble`: BLE advertising helpers on top of the NRF24 radio
  - `sdFat32`: FAT32 filesystem layer adapted from the standalone `sdFat32`
    project, wired to the local `SD_drv` block driver
  - `tft_gfx`: drawing, text, shape, and image helpers layered on top of `tft_drv`
- `samples/`
  - `esp32c6_blinky`: minimal LED blink sample for the local ESP32-C6 custom board
  - `z_esp32-wroom32`: ESP32-C6 demo combining OLED, NRF24, and SD card access
  - `central_multilink`: ESP32 demo for multilink BLE central experiments
  - `ble_peripheral`: BLE peripheral sample with the local OOT pieces
  - `z_nrf52_dongle`: nRF52 dongle sample
  - `z_stm32f401ccu6`: STM32F401 sample
- `boards/`
  - `esp32c6_custom`: local out-of-tree ESP32-C6 board with SH1106, NRF24, microSD, USB serial, and LED defaults

## Zephyr version

The repo is currently being exercised against Zephyr `4.3.0`.

The ESP32-family samples also need the Espressif blobs fetched in the Zephyr
workspace:

```sh
cd ~/zephyrproject
west blobs fetch hal_espressif
```

## Using this repo as a module

Each sample in `samples/` adds this repository through
`EXTRA_ZEPHYR_MODULES`, so the simplest workflow is to build from inside a
sample directory.

For a generic application outside this repo, point `EXTRA_ZEPHYR_MODULES` at the
repository root:

```sh
export EXTRA_ZEPHYR_MODULES=~/EmbeddedProjects/Zephyr-OOT
```

or append it from CMake:

```cmake
get_filename_component(OOT_MODULE_DIR "/absolute/path/to/Zephyr-OOT" REALPATH)
list(APPEND EXTRA_ZEPHYR_MODULES "${OOT_MODULE_DIR}")
```

After the module is added, use the public include namespace instead of including
headers directly from the implementation folders.

The module also exports an out-of-tree board root and devicetree bindings root,
so board targets like `esp32c6_custom/esp32c6/hpcore` and local compatibles
such as `surya,tft-spi` resolve through the module automatically.

## Sample highlights

### `samples/esp32c6_blinky`

Small sanity-check sample for `esp32c6_custom/esp32c6/hpcore`. It only needs
the board-provided `led0` alias and is a good first build when checking the
workspace, toolchain, and flash flow.

### `samples/z_esp32-wroom32`

Multi-peripheral ESP32-C6 demo that pulls in:

- `CONFIG_SD_FAT32`
- `CONFIG_OLED_SH1106`
- `CONFIG_SH1106_GFX`
- `CONFIG_NRF24_DRV`
- `CONFIG_NRF24_BLE`

The `esp32c6_custom` board definition enables:

- SH1106 on I2C
- SD card over SPI through the local `SD_drv`
- NRF24 over SPI
- `led0` alias for the onboard LED

### `drivers/tft_drv` and `subsys/tft_gfx`

The TFT stack is split into:

- `tft_drv` for the bus/controller layer
- `tft_gfx` for higher-level drawing helpers

The local binding compatible is `surya,tft-spi`, and public code should include
the APIs from:

- `#include <zephyr_oot/drivers/tft_drv.h>`
- `#include <zephyr_oot/subsys/tft_gfx.h>`

### `samples/central_multilink`

ESP32 BLE central demo. On Zephyr `4.3.0` it builds with:

```sh
west build -b esp32_devkitc/esp32/procpu
```

### `samples/z_stm32f401ccu6`

Carries the STM32-side OLED, SD, and NRF24 wiring used during bring-up. This
sample still assumes a board definition outside upstream Zephyr, so it may need
local board work before it is buildable in a fresh workspace.

## Build examples

From inside the sample directories:

```sh
source ~/zephyrproject/zephyr/zephyr-env.sh
west build -b esp32c6_custom/esp32c6/hpcore
```

For ESP32 targets, Zephyr also expects the Espressif Python tooling in the
active environment:

```sh
west packages pip --install
```

or use the provided helper scripts, for example:

```sh
cd samples/esp32c6_blinky
./build.sh
```

## Notes on `sdFat32`

`sdFat32` in this repo uses the local SPI SD block driver in `drivers/SD_drv`
as its storage backend.

That means the relevant sample configuration should enable the local SD driver,
for example:

```conf
CONFIG_SPI=y
CONFIG_SD_DRV=y
CONFIG_SD_FAT32=y
```
