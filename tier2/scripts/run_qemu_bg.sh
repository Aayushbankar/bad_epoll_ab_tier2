#!/bin/bash
./tier2/scripts/run_qemu.sh > /tmp/qemu.log 2>&1 &
echo $! > /tmp/qemu.pid
