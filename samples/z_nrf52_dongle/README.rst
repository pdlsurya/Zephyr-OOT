.. _z_nrf52_dongle_sample:

nRF52840 Dongle LED Sync Demo
#############################

Overview
********

This sample is a small board-specific threading demo for the nRF52840 dongle.

The main thread periodically releases a semaphore, and two worker threads race
to consume it and toggle separate LEDs. It is mainly useful as a quick
bring-up check for GPIO aliases, threading, and the local board overlay.

Building and Running
********************

Build it as follows:

.. code-block:: sh

   source ~/zephyrproject/zephyr/zephyr-env.sh
   west build -b nrf52840dongle/nrf52840

The overlay expects both ``led0`` and ``led1`` aliases to be valid for the
target board.
