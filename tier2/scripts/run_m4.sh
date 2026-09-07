#!/bin/bash
set -e
cd "$(dirname "$0")/.."

MODE=$1
if [ -z "$MODE" ]; then MODE="A"; fi

echo "[*] Compiling test_m4_cleanup.c for Mode ${MODE}..."
./aarch64-linux-musl-cross/bin/aarch64-linux-musl-gcc -DTEST_MODE=\'${MODE}\' -static -O0 -g -o rootfs/harness scripts/test_m4_cleanup.c

echo "[*] Packaging initramfs..."
cd rootfs
find . -print0 | cpio --null -ov --format=newc > ../initramfs.cpio
cd ..

rm -f qemu_serial_${MODE}.log
DEBUG=1 ./scripts/run_qemu.sh > /dev/null 2>&1 &
QEMU_PID=$!
sleep 2

TEST_MODE=$MODE gdb -batch -q -x scripts/exp_m4_gdb.py android/artifacts/vmlinux > evidence/m4_gdb_${MODE}.log 2>&1

kill $QEMU_PID || true
pkill -f qemu-system-aarch64 || true
cat qemu_serial.log > qemu_serial_${MODE}.log
cat qemu_serial_${MODE}.log | tail -n 50
