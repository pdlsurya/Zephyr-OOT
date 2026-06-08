.. _z_stm32f401ccu6_sample:

STM32F401 OLED and sdFat32 Demo
###############################

Overview
********

This sample is a board bring-up app for the STM32F401 wiring used in this
module.

It currently:

* blinks the board LED
* initializes ``sdFat32`` through the local ``SD_drv``
* shows console input on the SH1106 OLED

The overlay also carries NRF24 wiring used during bring-up, though the current
``main.c`` only exercises the display and storage paths.

Requirements
************

The sample is written around the ``blackpill_f401cc`` target with the local
overlay in this directory.

Building and Running
********************

Build it as follows:

.. code-block:: sh

   source ~/zephyrproject/zephyr/zephyr-env.sh
   west build -b blackpill_f401cc
   west flash

The overlay enables SH1106, microSD, and NRF24 on the STM32-side SPI/I2C pins
used by the sample.
