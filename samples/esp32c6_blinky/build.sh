source ~/zephyrproject/zephyr/zephyr-env.sh
west build -p auto -b esp32c6_custom/esp32c6/hpcore
west flash && west espressif monitor
