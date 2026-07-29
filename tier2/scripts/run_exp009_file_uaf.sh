#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." &> /dev/null && pwd)"
TIER2_DIR="${PROJECT_ROOT}/tier2"

echo "[*] Building EXP-009 test binary..."
cd "$TIER2_DIR"
./aarch64-linux-musl-cross/bin/aarch64-linux-musl-gcc -static -O0 -g -o rootfs/test_exp009 reproducers/test_exp009_file_uaf.c -pthread

echo "[*] Packaging rootfs..."
cd "$TIER2_DIR/rootfs" || exit 1
chmod +x init

find . -print0 | cpio --null -ov --format=newc > ../initramfs.cpio 2>/dev/null
cd "$TIER2_DIR"

echo "[*] Launching QEMU in background..."
DEBUG=1 ./scripts/run_qemu.sh > /tmp/qemu.log 2>&1 &
QEMU_PID=$!
sleep 5

echo "[*] Launching GDB automation..."
gdb -q -x scripts/gdb_exp009_file_uaf.py ./android/artifacts/vmlinux
