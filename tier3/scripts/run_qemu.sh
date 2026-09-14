#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." &> /dev/null && pwd)"
TIER3_DIR="${PROJECT_ROOT}/tier3"

KERNEL="${TIER3_DIR}/artifacts/Image"
RAMDISK="${TIER3_DIR}/initramfs.cpio"

CPUS=${CPUS:-2}
RAM=${RAM:-2048}
CMDLINE="console=ttyAMA0 root=/dev/ram0 rw"

QEMU_ARGS=(
    -M virt
    -cpu cortex-a57
    -smp "$CPUS"
    -m "$RAM"
    -kernel "$KERNEL"
    -initrd "$RAMDISK"
    -append "$CMDLINE"
    -display none
    -serial file:${TIER3_DIR}/evidence/qemu_serial_6.6.102.log
    -virtfs local,path=${TIER3_DIR}/evidence,mount_tag=host0,security_model=passthrough,id=host0
    -no-reboot
)

echo "Running QEMU..."
stdbuf -o0 -e0 qemu-system-aarch64 "${QEMU_ARGS[@]}" "$@"
