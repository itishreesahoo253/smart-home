#!/usr/bin/env bash
# Full demo with the kernel driver. Needs kernel headers + root.
set -e
cd "$(dirname "$0")/.."
make -C driver
sudo insmod driver/smarthome_drv.ko tick_ms=500
sudo chmod 666 /dev/smarthome
make -C daemon
trap 'sudo rmmod smarthome_drv; dmesg | tail -3' EXIT
./daemon/smarthome_daemon --dev /dev/smarthome --duration "${1:-60}" --log demo_log.csv
