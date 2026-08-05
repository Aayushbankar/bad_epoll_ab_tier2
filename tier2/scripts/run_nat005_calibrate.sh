#!/bin/bash
# run_nat005_calibrate.sh — Measures exact cycle count from close(outer) entry to __ep_remove hlist_del_rcu write
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
TIER2_DIR="$(cd "${SCRIPT_DIR}/.." &> /dev/null && pwd)"
cd "${TIER2_DIR}"

mkdir -p evidence

echo "[*] Compiling test_nat005_calibrate.c..."
./aarch64-linux-musl-cross/bin/aarch64-linux-musl-gcc -static -O0 -g -o rootfs/harness scripts/test_nat005_calibrate.c -pthread

echo "[*] Compiling init..."
./aarch64-linux-musl-cross/bin/aarch64-linux-musl-gcc -static -O2 -o rootfs/init rootfs/init.c

echo "[*] Packaging rootfs..."
cd rootfs
chmod +x init harness
find . -print0 | cpio --null -ov --format=newc > ../initramfs.cpio 2>/dev/null
cd "${TIER2_DIR}"

echo "[*] Launching QEMU in DEBUG mode for Calibration Measurement..."
rm -f evidence/NAT-005_calibration_raw.log

CMDLINE="console=ttyAMA0 root=/dev/ram0 kasan=off nokaslr earlycon=pl011,0x09000000 printk.devkmsg=on rw" DEBUG=1 ./scripts/run_qemu.sh > /dev/null 2>&1 &
QEMU_PID=$!
sleep 2

echo "[*] Executing GDB calibration script..."
gdb -batch -q -x scripts/exp_nat005_calibrate.py android/artifacts/vmlinux || true

echo "[*] Stopping QEMU..."
kill $QEMU_PID 2>/dev/null || true
pkill -f qemu-system-aarch64 2>/dev/null || true

echo "[*] Calibration Measurement Complete. Evidence file:"
ls -la evidence/NAT-005_calibration_raw.log
