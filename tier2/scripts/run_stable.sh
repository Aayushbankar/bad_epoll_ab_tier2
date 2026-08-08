#!/bin/bash
killall qemu-system-aarch64
# Updated 2026-08-08: use relative path after repo separation (was absolute bad-epoll-lab path)
cd "$(dirname "$0")/.."
DEBUG=1 ./scripts/run_qemu.sh > /tmp/qemu_run.log 2>&1 &
sleep 2
gdb -q -batch -x scripts/gdb_exp011_timerfd_pivot.py android/artifacts/vmlinux > evidence/EXP-011_raw_gdb.log 2>&1
