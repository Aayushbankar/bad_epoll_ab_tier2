#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." &> /dev/null && pwd)"
TIER2_DIR="${PROJECT_ROOT}/tier2"

echo "[*] Building EXP-007 test binary..."
cd "$TIER2_DIR"
./aarch64-linux-musl-cross/bin/aarch64-linux-musl-gcc -static -O2 -o rootfs/init reproducers/test_exp008_timing.c

echo "[*] Packaging rootfs..."
cd "$TIER2_DIR"
./scripts/build_rootfs.sh > /dev/null

echo "[*] Launching QEMU in background..."
DEBUG=1 ./scripts/run_qemu.sh > /tmp/qemu.log 2>&1 &
QEMU_PID=$!
sleep 2

echo "[*] Launching GDB automation..."
cd "$TIER2_DIR"
gdb -q -x scripts/gdb_exp008_timing.py ./android/artifacts/vmlinux

echo "[*] Cleaning up QEMU..."
kill -9 $QEMU_PID 2>/dev/null
