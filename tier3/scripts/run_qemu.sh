#!/bin/bash
# tier3/scripts/run_qemu.sh - Boot the Android 15 ARM64 6.6.102 GKI kernel using QEMU

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." &> /dev/null && pwd)"
TIER3_DIR="${PROJECT_ROOT}/tier3"

KERNEL="${TIER3_DIR}/artifacts/Image"
RAMDISK="${TIER3_DIR}/initramfs.cpio"

CPUS=${CPUS:-2}
RAM=${RAM:-2048}
CMDLINE=${CMDLINE:-"console=ttyAMA0 root=/dev/ram0 kasan=off nokaslr earlycon=pl011,0x09000000 printk.devkmsg=on rw"}

if [ ! -f "$KERNEL" ]; then
    echo "[!] ERROR: Kernel Image not found at $KERNEL"
    exit 1
fi

if [ ! -f "$RAMDISK" ]; then
    echo "[!] ERROR: Ramdisk not found at $RAMDISK."
    exit 1
fi

SERIAL=${SERIAL:-file:${TIER3_DIR}/evidence/qemu_serial_6.6.102.log}

QEMU_ARGS=(
    -M virt
    -cpu cortex-a57
    -smp "$CPUS"
    -m "$RAM"
    -kernel "$KERNEL"
    -initrd "$RAMDISK"
    -append "$CMDLINE"
    -display none
    -serial "$SERIAL"
    -no-reboot
)

if [ "$DEBUG" = "1" ]; then
    echo "[*] Launching in DEBUG mode. Waiting for GDB on :1234"
    QEMU_ARGS+=(-s -S)
fi

echo "=========================================================="
echo "[*] Launching QEMU Android 15 6.6.102 ARM64 Runtime"
echo "    Kernel:  $KERNEL"
echo "    Ramdisk: $RAMDISK"
echo "    CPUs: $CPUS | RAM: $RAM"
echo "    Cmdline: $CMDLINE"
echo "    Serial:  $SERIAL"
echo "=========================================================="

stdbuf -o0 -e0 qemu-system-aarch64 "${QEMU_ARGS[@]}" "$@"
