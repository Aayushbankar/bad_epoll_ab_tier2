#!/bin/bash
# run_hyp010_natural.sh — HYP-010 Part C: Natural Race Execution (N=5000)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." &> /dev/null && pwd)"
TIER2_DIR="${PROJECT_ROOT}/tier2"
CROSS="${TIER2_DIR}/aarch64-linux-musl-cross/bin/aarch64-linux-musl-gcc"
EVIDENCE_DIR="${TIER2_DIR}/evidence/HYP-010"

mkdir -p "${EVIDENCE_DIR}"

echo "============================================================"
echo "HYP-010 Part C: Natural Stage-1 Race Execution (N=5000)"
echo "Kernel: GKI 6.1.23 (Android 14 ARM64, nokaslr, QEMU TCG)"
echo "============================================================"

# 1. Compile init
echo "[1/4] Compiling init..."
${CROSS} -static -O2 -o "${TIER2_DIR}/rootfs/init" "${TIER2_DIR}/rootfs/init.c" \
    2>&1 || { echo "[!] init compile failed"; exit 1; }

# 2. Compile harness
echo "[2/4] Compiling test_hyp010_natural..."
${CROSS} -static -O2 -g -o "${TIER2_DIR}/rootfs/harness" "${TIER2_DIR}/scripts/test_hyp010_natural.c" \
    -pthread 2>&1 || { echo "[!] harness compile failed"; exit 1; }

# 3. Packaging initramfs
echo "[3/4] Packaging initramfs..."
cd "${TIER2_DIR}/rootfs"
chmod +x init harness
find . -print0 | cpio --null -ov --format=newc > "${TIER2_DIR}/initramfs.cpio" 2>/dev/null
cd "${TIER2_DIR}"

# 4. Launch QEMU (direct execution, non-debug)
echo "[4/4] Launching QEMU..."
pkill -f qemu-system-aarch64 || true
sleep 1

./scripts/run_qemu.sh > qemu_hyp010_natural.log 2>&1 &
QEMU_PID=$!
echo "[*] QEMU launched with PID ${QEMU_PID}, waiting for completion..."

# Wait for QEMU process to finish
wait ${QEMU_PID} || true
pkill -f qemu-system-aarch64 2>/dev/null || true

if [ -f "${TIER2_DIR}/qemu_serial.log" ]; then
    cp "${TIER2_DIR}/qemu_serial.log" "${EVIDENCE_DIR}/HYP-010_natural_serial.log" 2>/dev/null || true
fi

echo ""
echo "=== Execution Finished ==="
if [ -f "${EVIDENCE_DIR}/HYP-010_natural_serial.log" ]; then
    echo "=== Evidence Excerpt ==="
    tail -30 "${EVIDENCE_DIR}/HYP-010_natural_serial.log"
fi
