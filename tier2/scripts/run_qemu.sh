#!/bin/bash
# run_qemu.sh - Boot the Android ARM64 custom kernel using QEMU

# Resolve absolute paths robustly
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." &> /dev/null && pwd)"
TIER2_DIR="${PROJECT_ROOT}/tier2"

KERNEL="${TIER2_DIR}/android/artifacts/Image"
RAMDISK="${TIER2_DIR}/initramfs.cpio"

CPUS=${CPUS:-2}
RAM=${RAM:-2048}
CMDLINE=${CMDLINE:-"console=ttyAMA0 root=/dev/ram0 kasan=on nokaslr earlycon=pl011,0x09000000 printk.devkmsg=on rw"}

if [ ! -f "$KERNEL" ]; then
    echo "[!] ERROR: Kernel Image not found at $KERNEL"
    exit 1
fi

if [ ! -f "$RAMDISK" ]; then
    echo "[!] ERROR: Ramdisk not found at $RAMDISK. Run tier2/scripts/build_rootfs.sh first."
    exit 1
fi

QEMU_ARGS=(
    -M virt
    -cpu cortex-a57
    -smp "$CPUS"
    -m "$RAM"
    -kernel "$KERNEL"
    -initrd "$RAMDISK"
    -append "$CMDLINE"
    -display none
    -serial file:/tmp/qemu.log
    -no-reboot
)

if [ "$DEBUG" = "1" ]; then
    echo "[*] Launching in DEBUG mode. Waiting for GDB on :1234"
    QEMU_ARGS+=(-s -S)
fi

echo "=========================================================="
echo "[*] Launching QEMU Android ARM64 Runtime"
echo "    Kernel:  $KERNEL"
echo "    Ramdisk: $RAMDISK"
echo "    CPUs: $CPUS | RAM: $RAM"
echo "    Cmdline: $CMDLINE"
echo "=========================================================="
echo "    To exit QEMU at any time, press: Ctrl-a then x"
echo "=========================================================="
echo ""

qemu-system-aarch64 "${QEMU_ARGS[@]}"

