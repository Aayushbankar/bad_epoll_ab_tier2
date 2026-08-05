#!/bin/bash
# run_nat005.sh — Runs 100,000 iteration closed-loop adaptive search in QEMU
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
TIER2_DIR="$(cd "${SCRIPT_DIR}/.." &> /dev/null && pwd)"
cd "${TIER2_DIR}"

mkdir -p evidence

echo "[*] Compiling test_nat005.c..."
./aarch64-linux-musl-cross/bin/aarch64-linux-musl-gcc -static -O0 -g -o rootfs/harness scripts/test_nat005.c -pthread

echo "[*] Compiling init..."
./aarch64-linux-musl-cross/bin/aarch64-linux-musl-gcc -static -O2 -o rootfs/init rootfs/init.c

echo "[*] Packaging rootfs..."
cd rootfs
chmod +x init harness
find . -print0 | cpio --null -ov --format=newc > ../initramfs.cpio 2>/dev/null
cd "${TIER2_DIR}"

echo "[*] Launching QEMU in DEBUG mode for 100,000 Iteration Closed-Loop Search..."
rm -f evidence/NAT-005_raw_serial.log

CMDLINE="console=ttyAMA0 root=/dev/ram0 kasan=off nokaslr earlycon=pl011,0x09000000 printk.devkmsg=on isolcpus=1 nohz_full=1 rcu_nocbs=1 rw" DEBUG=1 ./scripts/run_qemu.sh > /dev/null 2>&1 &
QEMU_PID=$!
sleep 2

echo "[*] Executing GDB tracer script..."
gdb -batch -q -x scripts/exp_nat005_gdb.py android/artifacts/vmlinux || true

echo "[*] Stopping QEMU..."
kill $QEMU_PID 2>/dev/null || true
pkill -f qemu-system-aarch64 2>/dev/null || true

echo "[*] NAT-005 Closed-Loop Search complete. Evidence saved to evidence/NAT-005_raw_serial.log"
