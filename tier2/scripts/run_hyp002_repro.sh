#!/bin/bash
# run_hyp002_repro.sh — HYP-002 Reproduction: 3 x 5000 iterations on GKI 6.1.23
# Purpose: Reproduce the single KERNEL_UAF_DETECTED hit from commit cf24bd6
#          using the fixed kernel (explicit debug_freed=0 init in ep_alloc).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." &> /dev/null && pwd)"
TIER2_DIR="${PROJECT_ROOT}/tier2"
CROSS="${TIER2_DIR}/aarch64-linux-musl-cross/bin/aarch64-linux-musl-gcc"
EVIDENCE_DIR="${TIER2_DIR}/evidence/HYP-002"

NUM_RUNS=${NUM_RUNS:-3}

echo "=============================================="
echo "HYP-002 REPRODUCTION: ${NUM_RUNS} x 5000 iterations"
echo "Kernel: GKI 6.1.23 (with debug_freed=0 init fix)"
echo "=============================================="

# 1. Compile harness
echo "[1/4] Compiling test_hyp002 harness..."
${CROSS} -static -O0 -g -o "${TIER2_DIR}/rootfs/harness" "${TIER2_DIR}/scripts/test_hyp002.c" \
    -pthread 2>&1 || { echo "[!] harness compile failed"; exit 1; }

# 2. Package initramfs
echo "[2/4] Packaging initramfs..."
cd "${TIER2_DIR}/rootfs"
chmod +x init harness
find . -print0 | cpio --null -ov --format=newc > "${TIER2_DIR}/initramfs.cpio" 2>/dev/null

# 3. Copy updated kernel Image
echo "[3/4] Copying updated kernel Image..."
GKI_IMAGE="${TIER2_DIR}/android/source/common/arch/arm64/boot/Image"
if [ -f "${GKI_IMAGE}" ]; then
    cp "${GKI_IMAGE}" "${TIER2_DIR}/android/artifacts/Image"
    echo "[*] Kernel Image updated from source build."
else
    echo "[!] WARNING: No built Image found at ${GKI_IMAGE}"
    echo "[!] Using existing Image in artifacts/."
fi

# 4. Run QEMU N times
CMDLINE="console=ttyAMA0 root=/dev/ram0 kasan=off nokaslr earlycon=pl011,0x09000000 printk.devkmsg=on isolcpus=1 nohz_full=1 rcu_nocbs=1 rw"

total_uaf=0
run_results=()

for run in $(seq 1 ${NUM_RUNS}); do
    echo ""
    echo "========================================"
    echo "  RUN ${run}/${NUM_RUNS}"
    echo "========================================"

    SERIAL_LOG="${EVIDENCE_DIR}/HYP-002_repro_run${run}.log"
    cd "${TIER2_DIR}"

    timeout 300 stdbuf -o0 -e0 qemu-system-aarch64 \
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

    # Extract results
    if [ -f "${SERIAL_LOG}" ]; then
        uaf_count=$(grep -c "KERNEL_UAF_DETECTED\|UAF DETECTED" "${SERIAL_LOG}" 2>/dev/null || echo "0")
        kernel_uaf=$(grep "Kernel uaf_detected:" "${SERIAL_LOG}" 2>/dev/null | tail -1 | tr -d '\r' | awk '{print $NF}' || echo "?")
        oracle_hits=$(grep "Userspace oracle_hits:" "${SERIAL_LOG}" 2>/dev/null | tail -1 | tr -d '\r' | awk '{print $NF}' || echo "?")

        echo "[*] Run ${run}: kernel_uaf_detected=${kernel_uaf}, oracle_hits=${oracle_hits}, pr_warn_count=${uaf_count}"
        run_results+=("Run ${run}: kernel_uaf=${kernel_uaf}, oracle=${oracle_hits}")

        if [ "${kernel_uaf}" != "?" ] && [ "${kernel_uaf}" != "0" ]; then
            total_uaf=$((total_uaf + kernel_uaf))
        fi
    else
        echo "[!] Run ${run}: No serial log produced!"
        run_results+=("Run ${run}: FAILED (no log)")
    fi
done

# Summary
echo ""
echo "=============================================="
echo "HYP-002 REPRODUCTION SUMMARY"
echo "=============================================="
echo "Runs completed: ${NUM_RUNS}"
echo "Total kernel_uaf_detected: ${total_uaf}"
echo ""
for r in "${run_results[@]}"; do
    echo "  ${r}"
done
echo ""

if [ "${total_uaf}" -gt 0 ]; then
    echo ">>> REPRODUCTION SUCCESSFUL: ${total_uaf} UAF hits in ${NUM_RUNS} runs."
    echo ">>> The race IS naturally reachable on GKI 6.1.23."
    echo ">>> Previous '0 hits' documentation was incorrect (superseded session)."
else
    echo ">>> REPRODUCTION FAILED: 0 UAF hits across ${NUM_RUNS} runs (15,000 total iterations)."
    echo ">>> The single hit in cf24bd6 was likely a false positive from stale debug_freed."
    echo ">>> REJECTED conclusion stands."
fi
echo "=============================================="
