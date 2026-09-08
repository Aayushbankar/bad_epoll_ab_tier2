#!/bin/bash
cd "$(dirname "$0")/.."
echo "[*] Packaging initramfs..."
cd rootfs && chmod +x init harness && find . -print0 | cpio --null -ov --format=newc > ../initramfs.cpio && cd ..

echo "[*] Launching QEMU..."
DEBUG=1 ./scripts/run_qemu.sh > /dev/null 2>&1 &
QEMU_PID=$!
sleep 2

echo "[*] Connecting GDB..."
gdb -batch -q -x scripts/exp_hyp012_swaps_gdb.py android/artifacts/vmlinux

echo "[*] Stopping QEMU..."
kill $QEMU_PID 2>/dev/null || true
pkill -f qemu-system-aarch64 2>/dev/null || true
