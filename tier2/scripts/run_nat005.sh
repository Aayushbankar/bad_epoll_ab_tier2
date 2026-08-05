#!/bin/bash
# run_nat005.sh — NAT-005 Adaptive Launch-Ahead Search & Topology Test Launcher
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

echo "[*] Launching QEMU for NAT-005 Adaptive Search & Topology Verification..."
rm -f qemu_serial.log evidence/NAT-005_raw_serial.log

KERNEL="${TIER2_DIR}/android/artifacts/Image"
RAMDISK="${TIER2_DIR}/initramfs.cpio"
CMDLINE="console=ttyAMA0 root=/dev/ram0 kasan=off nokaslr earlycon=pl011,0x09000000 printk.devkmsg=on rw"

qemu-system-aarch64 \
    -M virt \
    -cpu cortex-a57 \
    -smp 2 \
    -m 2048 \
    -kernel "${KERNEL}" \
    -initrd "${RAMDISK}" \
    -append "${CMDLINE}" \
    -display none \
    -serial file:qemu_serial.log \
    -no-reboot

cp qemu_serial.log evidence/NAT-005_raw_serial.log
echo "[*] Evidence saved to evidence/NAT-005_raw_serial.log"
