#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." &> /dev/null && pwd)"
TIER2_DIR="${PROJECT_ROOT}/tier2"

cd "$TIER2_DIR"
./aarch64-linux-musl-cross/bin/aarch64-linux-musl-gcc -static -O0 -g -o rootfs/harness scripts/test_exp014.c -pthread

cd "$TIER2_DIR/rootfs" || exit 1
find . -print0 | cpio --null -ov --format=newc > ../initramfs.cpio 2>/dev/null
cd "$TIER2_DIR"

DEBUG=1 ./scripts/run_qemu.sh > /dev/null 2>&1 &
QEMU_PID=$!
sleep 2

gdb -batch -q -x scripts/gdb_exp014_trace.py android/artifacts/vmlinux > evidence/EXP-014_trace_gdb.log 2>&1

kill $QEMU_PID || true
pkill -f qemu-system-aarch64 || true
