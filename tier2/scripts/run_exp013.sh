#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." &> /dev/null && pwd)"
TIER2_DIR="${PROJECT_ROOT}/tier2"

echo "[*] Packaging rootfs..."
cd "$TIER2_DIR/rootfs" || exit 1
find * -print0 | cpio --null -ov --format=newc > ../initramfs.cpio 2>/dev/null
cd "$TIER2_DIR"

echo "[*] Launching QEMU in background with explicit -s -S..."
CMDLINE="console=ttyAMA0 oops=panic panic=-1 slub_debug=FZ nokaslr earlycon=pl011,0x09000000 rodata=off" 
qemu-system-aarch64 -machine virt,mte=off -cpu cortex-a57 -smp 2 -m 1G -kernel android/artifacts/Image -initrd initramfs.cpio -append "$CMDLINE" -nographic -no-reboot -gdb tcp::12345 > /tmp/my_qemu.log 2>&1 &
QEMU_PID=$!
echo $QEMU_PID > scripts/emulator.pid
