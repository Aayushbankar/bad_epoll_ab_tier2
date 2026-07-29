#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." &> /dev/null && pwd)"
TIER2_DIR="${PROJECT_ROOT}/tier2"

echo "[*] Building EXP-011 test binary..."
cd "$TIER2_DIR"
./aarch64-linux-musl-cross/bin/aarch64-linux-musl-gcc -static -O0 -g -o rootfs/test_exp011 reproducers/test_exp011_timerfd_pivot.c -pthread

echo "[*] Packaging rootfs..."
cd "$TIER2_DIR/rootfs" || exit 1
chmod +x init

# Copy test_exp011 to rootfs so init can run it
cp ../android/artifacts/rootfs/test_exp011 ./test_exp011

find . -print0 | cpio --null -ov --format=newc > ../initramfs.cpio 2>/dev/null
cd "$TIER2_DIR"

echo "[*] Launching QEMU in background..."
DEBUG=1 ./scripts/run_qemu.sh > /tmp/qemu.log 2>&1 &
QEMU_PID=$!
sleep 5

echo "[*] Launching GDB automation..."
gdb -q -ex "target remote :1234" -x scripts/gdb_exp011_timerfd_pivot.py ./android/artifacts/vmlinux
