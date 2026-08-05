#!/bin/bash
# run_and001.sh — AND-001 SysV IPC Availability Test Launcher
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
TIER2_DIR="$(cd "${SCRIPT_DIR}/.." &> /dev/null && pwd)"
cd "${TIER2_DIR}"

mkdir -p evidence

echo "[*] Compiling harness test_and001.c..."
./aarch64-linux-musl-cross/bin/aarch64-linux-musl-gcc -static -O0 -g -o rootfs/harness scripts/test_and001.c -pthread

echo "[*] Compiling init..."
./aarch64-linux-musl-cross/bin/aarch64-linux-musl-gcc -static -O2 -o rootfs/init rootfs/init.c

echo "[*] Packaging rootfs..."
cd rootfs
chmod +x init harness
find . -print0 | cpio --null -ov --format=newc > ../initramfs.cpio 2>/dev/null
cd "${TIER2_DIR}"

echo "[*] Launching QEMU in DEBUG mode..."
DEBUG=1 ./scripts/run_qemu.sh > /dev/null 2>&1 &
QEMU_PID=$!
sleep 3

echo "[*] Running AND-001 GDB script..."
gdb -batch -q -x scripts/exp_and001_gdb.py android/artifacts/vmlinux || true

echo "[*] Stopping QEMU..."
kill $QEMU_PID 2>/dev/null || true
pkill -f qemu-system-aarch64 2>/dev/null || true
sleep 1

echo "[*] GDB execution complete. Checking evidence file..."
