#!/bin/bash
# run_emulator.sh - Boot the Android ARM64 emulator with custom kernel

EMULATOR="$HOME/.local/android/emulator/emulator"
AVD="API_34_ARM64"
KERNEL="$(pwd)/../android/artifacts/Image"
EXTRA_CMDLINE="kasan=off earlycon=pl011,0x09000000 printk.devkmsg=on"

echo "[*] Launching emulator $AVD with custom kernel"
echo "[*] Kernel: $KERNEL"
echo "[*] Cmdline: $EXTRA_CMDLINE"

$EMULATOR -avd $AVD \
    -kernel "$KERNEL" \
    -no-window \
    -no-audio \
    -show-kernel \
    -verbose \
    -qemu -append "$EXTRA_CMDLINE"
