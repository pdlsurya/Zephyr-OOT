source ~/zephyrproject/zephyr/zephyr-env.sh
west build -p auto -b esp32c6_devkitc/esp32c6/hpcore
west flash && west espressif monitor
