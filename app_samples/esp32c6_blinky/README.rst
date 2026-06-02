ESP32-C6 Blinky
###############

Overview
********

Minimal ESP32-C6 LED blink sample for the local out-of-tree Zephyr module
workspace.

The sample is intentionally small and is useful for checking that:

* the Zephyr workspace is configured correctly
* the ESP32-C6 board target resolves
* flashing and monitor flow work before moving to the larger demos

Building and Running
********************

Use the board target below with Zephyr 4.3:

.. code-block:: sh

   source ~/zephyrproject/zephyr/zephyr-env.sh
   west build -b esp32c6_devkitc/esp32c6/hpcore
   west flash
   west espressif monitor

The sample provides its own ``led0`` alias through ``app.overlay`` for the
onboard LED on GPIO15.
