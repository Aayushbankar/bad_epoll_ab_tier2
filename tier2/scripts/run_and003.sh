#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
TIER2_DIR="$(cd "${SCRIPT_DIR}/.." &> /dev/null && pwd)"
cd "${TIER2_DIR}"

mkdir -p evidence
rm -f evidence/AND-003_raw_enforcing.log

echo "[*] Compiling test_and003.c..."
./aarch64-linux-musl-cross/bin/aarch64-linux-musl-gcc -static -O0 -g -o rootfs/harness scripts/test_and003.c

echo "[*] Packaging rootfs..."
cd rootfs
chmod +x init harness
find . -print0 | cpio --null -ov --format=newc > ../initramfs.cpio 2>/dev/null
cd "${TIER2_DIR}"

echo "[*] Launching QEMU in DEBUG mode..."

CMDLINE="console=ttyAMA0 root=/dev/ram0 kasan=off nokaslr earlycon=pl011,0x09000000 printk.devkmsg=on rw security=selinux selinux=1 enforcing=1" DEBUG=1 ./scripts/run_qemu.sh > /dev/null 2>&1 &
QEMU_PID=$!
sleep 2

echo "[*] Executing GDB tracer script..."
gdb -batch -q -x scripts/exp_and003_gdb.py android/artifacts/vmlinux || true

echo "[*] Stopping QEMU..."
kill $QEMU_PID 2>/dev/null || true
pkill -f qemu-system-aarch64 2>/dev/null || true

echo "[*] AND-003 Search complete. Evidence saved to evidence/AND-003_raw_enforcing.log"
