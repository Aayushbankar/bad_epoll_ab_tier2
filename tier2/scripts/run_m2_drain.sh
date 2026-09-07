#!/bin/bash
set -e
cd "$(dirname "$0")/.."

echo "[*] Compiling rootfs init..."
./aarch64-linux-musl-cross/bin/aarch64-linux-musl-gcc -static -O0 -g -o rootfs/init rootfs/init.c

echo "[*] Compiling drain test..."
./aarch64-linux-musl-cross/bin/aarch64-linux-musl-gcc -static -O0 -g -o rootfs/harness scripts/test_m2_drain.c

echo "[*] Packaging initramfs..."
cd rootfs
find . -print0 | cpio --null -ov --format=newc > ../initramfs.cpio
cd ..

echo "[*] Launching QEMU..."
rm -f qemu_serial.log
./scripts/run_qemu.sh > /dev/null 2>&1 &
QEMU_PID=$!
sleep 15
kill $QEMU_PID || true
pkill -f qemu-system-aarch64 || true
cat qemu_serial.log | grep -A 20 "Dropped to UID 2000"
