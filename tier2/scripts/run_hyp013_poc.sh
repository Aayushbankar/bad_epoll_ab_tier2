#!/bin/bash
# run_hyp013_poc.sh — Execute unassisted HYP-013 PoC

set -e
cd "$(dirname "$0")/.."

echo "[*] Compiling test_hyp013_poc.c (static musl)..."
./aarch64-linux-musl-cross/bin/aarch64-linux-musl-gcc -static -O0 -g -o rootfs/harness scripts/test_hyp013_poc.c -pthread

echo "[*] Packaging initramfs..."
cd rootfs
find . -print0 | cpio --null -ov --format=newc > ../initramfs.cpio
cd ..

echo "[*] Launching QEMU for unassisted PoC (No GDB)..."
rm -f evidence/HYP-013/HYP-013_poc.log
mkdir -p evidence/HYP-013

./scripts/run_qemu.sh > evidence/HYP-013/HYP-013_poc.log 2>&1

echo "[*] Run complete. Check logs in evidence/HYP-013/HYP-013_poc.log"
