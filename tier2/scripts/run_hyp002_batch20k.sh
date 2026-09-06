#!/bin/bash
# run_hyp002_batch20k.sh — Clean, Continuous 20,000-Iteration Reproduction Batch
# Purpose: Quantify empirical hit rate and confidence interval for HYP-002 on GKI 6.1.23
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." &> /dev/null && pwd)"
TIER2_DIR="${PROJECT_ROOT}/tier2"
CROSS="${TIER2_DIR}/aarch64-linux-musl-cross/bin/aarch64-linux-musl-gcc"
EVIDENCE_DIR="${TIER2_DIR}/evidence/HYP-002"
SERIAL_LOG="${EVIDENCE_DIR}/HYP-002_repro_batch20k.log"

ITERATIONS=${ITERATIONS:-20000}

echo "=============================================="
echo "HYP-002 CONTINUOUS BATCH: ${ITERATIONS} iterations"
echo "Target Kernel: Android GKI 6.1.23 (TCG virt)"
echo "Output Log: ${SERIAL_LOG}"
echo "=============================================="

# 1. Compile test_hyp002 harness with ITERATIONS=20000
echo "[1/4] Compiling test_hyp002 harness with -DITERATIONS=${ITERATIONS}..."
${CROSS} -static -O0 -g -DITERATIONS=${ITERATIONS} \
    -o "${TIER2_DIR}/rootfs/harness" "${TIER2_DIR}/scripts/test_hyp002.c" \
    -pthread 2>&1 || { echo "[!] harness compile failed"; exit 1; }

# 2. Package initramfs
echo "[2/4] Packaging initramfs..."
cd "${TIER2_DIR}/rootfs"
chmod +x init harness
find . -print0 | cpio --null -ov --format=newc > "${TIER2_DIR}/initramfs.cpio" 2>/dev/null

# 3. Ensure updated kernel Image is in artifacts
echo "[3/4] Checking kernel Image..."
GKI_IMAGE="${TIER2_DIR}/android/source/common/arch/arm64/boot/Image"
if [ -f "${GKI_IMAGE}" ]; then
    cp "${GKI_IMAGE}" "${TIER2_DIR}/android/artifacts/Image"
    echo "[*] Kernel Image synced from source build."
fi

# 4. Run QEMU for single continuous batch
echo "[4/4] Launching QEMU for continuous ${ITERATIONS}-iteration run..."
CMDLINE="console=ttyAMA0 root=/dev/ram0 kasan=off nokaslr earlycon=pl011,0x09000000 printk.devkmsg=on isolcpus=1 nohz_full=1 rcu_nocbs=1 rw"

cd "${TIER2_DIR}"
rm -f "${SERIAL_LOG}"

timeout 900 stdbuf -o0 -e0 qemu-system-aarch64 \
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
echo "=============================================="
echo "BATCH EXECUTION FINISHED"
echo "=============================================="

if [ -f "${SERIAL_LOG}" ]; then
    kernel_uaf=$(grep "Kernel uaf_detected:" "${SERIAL_LOG}" 2>/dev/null | tail -1 | tr -d '\r' | awk '{print $NF}' || echo "?")
    oracle_hits=$(grep "Userspace oracle_hits:" "${SERIAL_LOG}" 2>/dev/null | tail -1 | tr -d '\r' | awk '{print $NF}' || echo "?")
    uaf_warnings=$(grep -c "UAF DETECTED" "${SERIAL_LOG}" 2>/dev/null || echo "0")

    echo "[*] Results: kernel_uaf_detected=${kernel_uaf}, oracle_hits=${oracle_hits}, pr_warn_count=${uaf_warnings}"
else
    echo "[!] ERROR: No serial log produced at ${SERIAL_LOG}"
fi
