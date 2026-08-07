#!/bin/bash
set -e

echo "[*] Compiling verify_caches..."
./aarch64-linux-musl-cross/bin/aarch64-linux-musl-gcc -static -O0 -g -o rootfs/harness scripts/verify_caches.c -pthread

echo "[*] Compiling init..."
./aarch64-linux-musl-cross/bin/aarch64-linux-musl-gcc -static -O2 -o rootfs/init rootfs/init.c

echo "[*] Packaging rootfs..."
cd rootfs
find . -print0 | cpio --null -ov --format=newc > ../initramfs.cpio 2>/dev/null
cd ..

echo "[*] Launching QEMU..."
./scripts/run_qemu.sh &
QEMU_PID=$!

sleep 15
kill $QEMU_PID || true

cat /tmp/qemu.log | grep -A 10 "Starting Cache Verification"
