#!/bin/bash
# run_exp029.sh — Build & run the EXP-029 corrected E2E PoC (unassisted, no GDB)
set -e
cd "$(dirname "$0")/.."

echo "[*] Compiling test_exp029_e2e.c..."
./aarch64-linux-musl-cross/bin/aarch64-linux-musl-gcc -static -O0 -g \
    -o rootfs/harness scripts/test_exp029_e2e.c -pthread

echo "[*] Compiling init.c..."
./aarch64-linux-musl-cross/bin/aarch64-linux-musl-gcc -static -O0 -g \
    -o rootfs/init rootfs/init.c

echo "[*] Packaging initramfs..."
cd rootfs
chmod +x init harness
find . -print0 | cpio --null -ov --format=newc > ../initramfs.cpio 2>/dev/null
cd ..

echo "[*] Launching QEMU (no GDB, unassisted)..."
# No DEBUG=1, no -s -S. Pure unassisted run.
SERIAL=stdio ./scripts/run_qemu.sh 2>&1 | tee evidence/EXP-029/EXP-029_raw_serial.log

echo "[*] QEMU exited. Raw serial log saved to evidence/EXP-029/EXP-029_raw_serial.log"
