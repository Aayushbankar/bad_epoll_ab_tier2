#!/bin/bash
# run_gdb_timer_write_test.sh

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
TIER2_DIR="$(cd "${SCRIPT_DIR}/.." &> /dev/null && pwd)"

KERNEL="${TIER2_DIR}/android/artifacts/Image"
RAMDISK="${TIER2_DIR}/initramfs.cpio"
VMLINUX="${TIER2_DIR}/android/artifacts/vmlinux"

echo "[*] Starting QEMU with GDB server (-s -S)..."
qemu-system-aarch64 \
    -M virt \
    -cpu cortex-a57 \
    -smp 2 \
    -m 2048 \
    -kernel "$KERNEL" \
    -initrd "$RAMDISK" \
    -append "console=ttyAMA0 root=/dev/ram0 kasan=off nokaslr earlycon=pl011,0x09000000 printk.devkmsg=on rw" \
    -nographic \
    -no-reboot \
    -s -S > /tmp/qemu_gdb_timer_write.log 2>&1 &

QEMU_PID=$!
echo "[*] QEMU launched PID=$QEMU_PID. Waiting 2 seconds..."
sleep 2

echo "[*] Running GDB stale write modification experiment..."
gdb -batch \
    -ex "file $VMLINUX" \
    -ex "source ${SCRIPT_DIR}/gdb_timer_write_test.py" \
    -ex "python run_experiment()"

echo "[*] Cleaning up QEMU..."
kill -9 $QEMU_PID 2>/dev/null || true
echo "[*] Done."
