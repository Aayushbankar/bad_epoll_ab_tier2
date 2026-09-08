#!/bin/bash
set -e
cd "$(dirname "$0")/.."
echo "[*] Compiling test_m5_poll.c..."
./aarch64-linux-musl-cross/bin/aarch64-linux-musl-gcc -static -O0 -g -o rootfs/harness scripts/test_m5_poll.c
echo "[*] Packaging initramfs..."
cd rootfs
find . -print0 | cpio --null -ov --format=newc > ../initramfs.cpio 2>/dev/null
cd ..
DEBUG=1 ./scripts/run_qemu.sh > qemu_serial.log 2>&1 &
QEMU_PID=$!
sleep 0.5
stdbuf -o0 -e0 gdb -batch -q -x scripts/exp_m5_gdb.py android/artifacts/vmlinux > evidence/m5_gdb2.log 2>&1
kill $QEMU_PID 2>/dev/null || true
pkill -f qemu-system-aarch64 2>/dev/null || true
