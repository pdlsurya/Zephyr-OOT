.. _ble_peripheral_display_sample:

Bluetooth Peripheral With SH1106 Display
########################################

Overview
********

This sample exposes a custom Bluetooth LE GATT service and uses the local
``sh1106_gfx`` subsystem to show runtime status on an SH1106 OLED.

When a central connects, disconnects, or writes to the writable characteristic,
the event is logged to both the console and the display.


Requirements
************

The sample expects:

* a board with Zephyr Bluetooth LE peripheral support
* an SH1106 OLED wired for the selected board target
* the local OOT module enabled through ``EXTRA_ZEPHYR_MODULES`` when building
  the sample directly

Building and Running
********************

Example build targets:

.. code-block:: sh

   source ~/zephyrproject/zephyr/zephyr-env.sh
   west build -b esp32c6_custom/esp32c6/hpcore

or:

.. code-block:: sh

   source ~/zephyrproject/zephyr/zephyr-env.sh
   west build -b nrf52840dongle/nrf52840
