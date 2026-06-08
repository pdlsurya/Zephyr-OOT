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
   export PATH="$(python3 -m site --user-base)/bin:$PATH"
   west build -b esp32c6_custom/esp32c6/hpcore
   west flash
   west espressif monitor

The ``esp32c6_custom`` board definition provides the ``led0`` alias on GPIO15.
If ``west build`` reports ``esptool>=5.0.2 not found in PATH``, the Python
package is usually installed already and only the user Python ``bin`` directory
needs to be added to ``PATH`` as shown above.
