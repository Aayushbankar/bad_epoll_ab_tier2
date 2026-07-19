#!/bin/bash
# build_rootfs.sh - Pack the initramfs for the Android ARM64 runtime.
# This script enables a fast userspace test workflow.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
TIER2_DIR="$(cd "${SCRIPT_DIR}/.." &> /dev/null && pwd)"
ROOTFS_DIR="${TIER2_DIR}/rootfs"
RAMDISK_OUT="${TIER2_DIR}/initramfs.cpio"

if [ ! -d "$ROOTFS_DIR" ]; then
    echo "[!] ERROR: rootfs directory not found at $ROOTFS_DIR"
    exit 1
fi

echo "[*] Packaging initramfs from $ROOTFS_DIR"
cd "$ROOTFS_DIR" || exit 1
find . -print0 | cpio --null -ov --format=newc > "$RAMDISK_OUT"

echo "[*] Ramdisk generated at: $RAMDISK_OUT"
echo "[*] Success."
