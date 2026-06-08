source ~/zephyrproject/zephyr/zephyr-env.sh
west build -p always -b esp32c6_custom/esp32c6/hpcore
west flash
