#!/bin/bash
# collect_runtime_evidence.sh - Captures objective evidence of the runtime state

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
TIER2_DIR="$(cd "${SCRIPT_DIR}/.." &> /dev/null && pwd)"
EVIDENCE_ROOT="${TIER2_DIR}/evidence"
TIMESTAMP=$(date -u +"%Y-%m-%dT%H%M%SZ")
EVIDENCE_DIR="${EVIDENCE_ROOT}/${TIMESTAMP}"

KERNEL="${TIER2_DIR}/android/artifacts/Image"
RAMDISK="${TIER2_DIR}/initramfs.cpio"
VMLINUX="${TIER2_DIR}/android/artifacts/vmlinux"

mkdir -p "$EVIDENCE_DIR"
echo "[*] Collecting evidence to $EVIDENCE_DIR"

echo "=== Artifact Hashes ===" > "${EVIDENCE_DIR}/hashes.txt"
sha256sum "$KERNEL" >> "${EVIDENCE_DIR}/hashes.txt"
sha256sum "$RAMDISK" >> "${EVIDENCE_DIR}/hashes.txt"
sha256sum "$VMLINUX" >> "${EVIDENCE_DIR}/hashes.txt"

echo "=== Launch Configuration ===" > "${EVIDENCE_DIR}/launch_config.txt"
echo "CPUS: ${CPUS:-2}" >> "${EVIDENCE_DIR}/launch_config.txt"
echo "RAM: ${RAM:-2048}" >> "${EVIDENCE_DIR}/launch_config.txt"
echo "CMDLINE: ${CMDLINE:-console=ttyAMA0 root=/dev/ram0 kasan=off earlycon=pl011,0x09000000 printk.devkmsg=on rw}" >> "${EVIDENCE_DIR}/launch_config.txt"

echo "[*] Launching QEMU headless to collect dmesg and system info..."
# Modify the rootfs init script temporarily or just use QEMU to dump boot logs.
# Since our custom init already prints the required info, we just redirect output.

timeout 60 "${SCRIPT_DIR}/run_qemu.sh" > "${EVIDENCE_DIR}/qemu_output.log" 2>&1

if grep -q "Linux version" "${EVIDENCE_DIR}/qemu_output.log"; then
    echo "[+] Boot successful. Evidence captured."
    echo "SUCCESS" > "${EVIDENCE_DIR}/status.txt"
else
    echo "[-] Boot failed or timed out."
    echo "FAILED" > "${EVIDENCE_DIR}/status.txt"
fi

echo "[*] Evidence collection complete."
