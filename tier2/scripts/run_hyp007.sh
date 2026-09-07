#!/bin/bash
# run_hyp007.sh — HYP-007: Struct File UAF Race Detection Test
# Builds harness, packages initramfs, runs QEMU, captures evidence.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." &> /dev/null && pwd)"
TIER2_DIR="${PROJECT_ROOT}/tier2"
CROSS="${TIER2_DIR}/aarch64-linux-musl-cross/bin/aarch64-linux-musl-gcc"
EVIDENCE_DIR="${TIER2_DIR}/evidence/HYP-007"

mkdir -p "${EVIDENCE_DIR}"

echo "=============================================="
echo "HYP-007: Struct File UAF Detection (5000 iters)"
echo "Kernel: GKI 6.1.23 (with HYP-007 instrumentation)"
echo "=============================================="

# 1. Compile init with debugfs mount support
echo "[1/4] Compiling init..."
${CROSS} -static -O2 -o "${TIER2_DIR}/rootfs/init" "${TIER2_DIR}/rootfs/init.c" \
    2>&1 || { echo "[!] init compile failed"; exit 1; }

# 2. Compile harness
echo "[2/4] Compiling test_hyp007..."
${CROSS} -static -O0 -g -o "${TIER2_DIR}/rootfs/harness" "${TIER2_DIR}/scripts/test_hyp007.c" \
    -pthread 2>&1 || { echo "[!] harness compile failed"; exit 1; }

# 3. Package initramfs
echo "[3/4] Packaging initramfs..."
cd "${TIER2_DIR}/rootfs"
chmod +x init harness
find . -print0 | cpio --null -ov --format=newc > "${TIER2_DIR}/initramfs.cpio" 2>/dev/null

# 4. Sync kernel Image
GKI_IMAGE="${TIER2_DIR}/android/source/common/arch/arm64/boot/Image"
if [ -f "${GKI_IMAGE}" ]; then
    cp "${GKI_IMAGE}" "${TIER2_DIR}/android/artifacts/Image"
    echo "[*] Kernel Image updated from source build."
fi

# 5. Run QEMU
echo "[4/4] Launching QEMU..."
SERIAL_LOG="${EVIDENCE_DIR}/HYP-007_raw_serial.log"
cd "${TIER2_DIR}"

CMDLINE="console=ttyAMA0 root=/dev/ram0 kasan=off nokaslr earlycon=pl011,0x09000000 printk.devkmsg=on isolcpus=1 nohz_full=1 rcu_nocbs=1 rw"

timeout 600 stdbuf -o0 -e0 qemu-system-aarch64 \
    -M virt \
    -cpu cortex-a57 \
    -smp 2 \
    -m 2048 \
    -kernel android/artifacts/Image \
    -initrd initramfs.cpio \
    -append "${CMDLINE}" \
    -display none \
    -serial "file:${SERIAL_LOG}" \
    -no-reboot \
    2>&1 || true

echo ""
echo "=== Execution Finished ==="
if [ -f "${SERIAL_LOG}" ]; then
    tail -35 "${SERIAL_LOG}"
else
    echo "[!] No serial log produced!"
fi
