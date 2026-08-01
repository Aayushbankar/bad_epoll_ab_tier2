#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." &> /dev/null && pwd)"
TIER2_DIR="${PROJECT_ROOT}/tier2"

mkdir -p "$TIER2_DIR/evidence"

echo "[*] Compiling harness..."
cd "$TIER2_DIR"
./aarch64-linux-musl-cross/bin/aarch64-linux-musl-gcc -static -O0 -g -o rootfs/harness scripts/test_exp023b.c -pthread

echo "[*] Compiling init..."
./aarch64-linux-musl-cross/bin/aarch64-linux-musl-gcc -static -O2 -o rootfs/init rootfs/init.c

echo "[*] Packaging rootfs..."
cd "$TIER2_DIR/rootfs" || exit 1
chmod +x init
chmod +x harness
find . -print0 | cpio --null -ov --format=newc > ../initramfs.cpio 2>/dev/null
cd "$TIER2_DIR"

echo "[*] Launching QEMU..."
DEBUG=1 ./scripts/run_qemu.sh 2>&1 | tee /tmp/qemu_exp023b.log &
QEMU_PID=$!

echo "[*] QEMU launched with PID $QEMU_PID"
sleep 2

echo "[*] Running EXP-023b GDB script..."
gdb -batch -q -x scripts/exp023b_gdb.py android/artifacts/vmlinux

sleep 5

echo "[*] Stopping emulator..."
kill $QEMU_PID || true
pkill -f qemu-system-aarch64 || true

echo "[*] QEMU log saved to /tmp/qemu_exp023b.log"
tail -50 /tmp/qemu_exp023b.log