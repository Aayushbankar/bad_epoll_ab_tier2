#!/bin/bash
cd "$(dirname "$0")"/..
rm -f test_qemu_serial.log
SERIAL="file:test_qemu_serial.log" ./scripts/run_qemu.sh > /dev/null 2>&1 &
QEMU_PID=$!
sleep 15
pkill -f qemu-system-aarch64 || true
cat test_qemu_serial.log
