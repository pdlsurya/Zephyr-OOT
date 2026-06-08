source ~/zephyrproject/zephyr/zephyr-env.sh
west build -p always -b esp32_devkitc/esp32/procpu
west flash
