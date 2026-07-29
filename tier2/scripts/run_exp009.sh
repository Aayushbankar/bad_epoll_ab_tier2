#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." &> /dev/null && pwd)"
TIER2_DIR="${PROJECT_ROOT}/tier2"

echo "[*] Building EXP-009 test binary..."
cd "$PROJECT_ROOT"
# FORCE RECOMPILE
rm -f tier2/rootfs/init
./tier2/aarch64-linux-musl-cross/bin/aarch64-linux-musl-gcc -static -O0 -g \
    tier2/reproducers/test_exp009_file_uaf.c -o tier2/rootfs/init \
    -pthread
chmod +x tier2/rootfs/init

echo "[*] Packaging rootfs..."
cd "$TIER2_DIR/rootfs" || exit 1
find * -print0 | cpio --null -ov --format=newc > ../initramfs.cpio 2>/dev/null
cd "$TIER2_DIR"

echo "[*] Launching QEMU in background with explicit -s -S..."
CMDLINE="console=ttyAMA0 root=/dev/ram0 kasan=on nokaslr earlycon=pl011,0x09000000 printk.devkmsg=on rw" 
qemu-system-aarch64 -M virt -cpu cortex-a57 -smp 2 -m 2048 \
    -kernel android/artifacts/Image \
    -initrd initramfs.cpio \
    -append "$CMDLINE" -nographic -no-reboot -s -S &
QEMU_PID=$!

sleep 2

echo "[*] Launching GDB automation..."
cat << 'GDB' > /tmp/run_gdb_exp009.gdb
source scripts/gdb_exp009_file_uaf.py
GDB

gdb -q -batch -ex "file android/artifacts/vmlinux" -x /tmp/run_gdb_exp009.gdb 2>&1 | tee evidence/EXP-009_raw_gdb.log

kill $QEMU_PID 2>/dev/null || true
