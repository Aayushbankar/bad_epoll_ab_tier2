#!/bin/bash
# run_qemu_virtio.sh - Boot with virtio console for logging

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." &> /dev/null && pwd)"
TIER2_DIR="${PROJECT_ROOT}/tier2"

KERNEL="${TIER2_DIR}/android/artifacts/Image"
RAMDISK="${TIER2_DIR}/initramfs.cpio"

CPUS=${CPUS:-2}
RAM=${RAM:-2048}
CMDLINE=${CMDLINE:-"console=ttyAMA0 root=/dev/ram0 kasan=off nokaslr earlycon=pl011,0x09000000 printk.devkmsg=on isolcpus=1 nohz_full=1 rcu_nocbs=1 rw"}

QEMU_ARGS=(
    -M virt
    -cpu cortex-a57
    -smp "$CPUS"
    -m "$RAM"
    -kernel "$KERNEL"
    -initrd "$RAMDISK"
    -device virtio-serial-device
    -chardev file,id=char1,path="${TIER2_DIR}/evidence/AND-002_raw_kaslr_on_virtio.log"
    -device virtconsole,chardev=char1
    -append "$CMDLINE"
    -display none
    -serial null
    -no-reboot
)

stdbuf -o0 -e0 qemu-system-aarch64 "${QEMU_ARGS[@]}"
