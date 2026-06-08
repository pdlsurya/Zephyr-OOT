.. _z_esp32_wroom32_sample:

ESP32-C6 OLED, NRF24, and sdFat32 Demo
######################################

Overview
********

This sample is a board bring-up and integration app for the local out-of-tree
modules on ``esp32c6_custom``.

At the moment it:

* blinks the board LED from a worker thread
* initializes ``nrf24_ble`` on top of the local ``nrf24_drv``
* initializes ``sdFat32`` on top of the local ``SD_drv``
* mirrors console input to the SH1106 OLED through ``sh1106_gfx``

Requirements
************

The sample expects the local ``esp32c6_custom/esp32c6/hpcore`` board
definition, which carries the OLED, SD, NRF24, USB serial console, and LED
wiring.

Building and Running
********************

Build it as follows:

.. code-block:: sh

   source ~/zephyrproject/zephyr/zephyr-env.sh
   west build -b esp32c6_custom/esp32c6/hpcore
   west flash
   west espressif monitor

The board definition enables:

* SH1106 on ``i2c0``
* microSD over SPI through ``SD_drv``
* NRF24 over SPI through ``nrf24_drv``
* ``led0`` for the onboard LED
