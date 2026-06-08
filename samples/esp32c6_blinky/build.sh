#!/usr/bin/env bash

set -euo pipefail

source ~/zephyrproject/zephyr/zephyr-env.sh

# Zephyr's Espressif support looks for the `esptool` executable, which is
# commonly installed into the user Python bin directory on macOS.
PYTHON_USER_BIN="$(python3 -m site --user-base)/bin"
if [ -d "${PYTHON_USER_BIN}" ]; then
  export PATH="${PYTHON_USER_BIN}:${PATH}"
fi

west build -p auto -b esp32c6_custom/esp32c6/hpcore
west flash && west espressif monitor
