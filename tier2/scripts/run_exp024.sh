#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." &> /dev/null && pwd)"
TIER2_DIR="${PROJECT_ROOT}/tier2"

# Ensure evidence directory exists
mkdir -p "$TIER2_DIR/evidence"

# Kill any existing QEMU
pkill -f qemu-system-aarch64 2>/dev/null || true
sleep 1

echo "[*] Compiling EXP-024 harness..."
cd "$TIER2_DIR"
./aarch64-linux-musl-cross/bin/aarch64-linux-musl-gcc -static -O0 -g \
    -o rootfs/harness scripts/test_exp024.c -pthread

echo "[*] Compiling init..."
./aarch64-linux-musl-cross/bin/aarch64-linux-musl-gcc -static -O2 \
    -o rootfs/init rootfs/init.c

echo "[*] Packaging rootfs..."
cd "$TIER2_DIR/rootfs" || exit 1
chmod +x init
chmod +x harness
find . -print0 | cpio --null -ov --format=newc > ../initramfs.cpio 2>/dev/null
cd "$TIER2_DIR"

echo "[*] Launching QEMU..."
DEBUG=1 ./scripts/run_qemu.sh 2>&1 | tee /tmp/qemu_exp024_serial.log &
QEMU_PID=$!

echo "[*] QEMU launched with PID $QEMU_PID"
sleep 2

echo "[*] Running EXP-024 GDB script..."
timeout 120 gdb -batch -q -x scripts/exp024_gdb.py android/artifacts/vmlinux || true

# Wait for harness to complete
sleep 5

echo "[*] Stopping emulator..."
kill $QEMU_PID 2>/dev/null || true
pkill -f qemu-system-aarch64 2>/dev/null || true

echo ""
echo "============================================"
echo "[*] QEMU serial output (last 80 lines):"
echo "============================================"
tail -80 /tmp/qemu_exp024_serial.log

echo ""
echo "============================================"
echo "[*] GDB log:"
echo "============================================"
cat "$TIER2_DIR/evidence/EXP-024_raw_gdb.log" 2>/dev/null || echo "No GDB log found"
