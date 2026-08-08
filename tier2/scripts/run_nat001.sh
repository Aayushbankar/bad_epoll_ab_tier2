#!/bin/bash
# run_nat001.sh — NAT-001 Statistical Natural Race Test Launcher
# Runs 10 boots × 1000 iterations = 10,000 total
# Each boot: compile → package rootfs → QEMU → run harness → collect evidence

set -euo pipefail

# Updated 2026-08-08: use relative path after repo separation (was absolute bad-epoll-lab path)
TIER2_DIR="$(cd "$(dirname "$0")/.." && pwd)"
EVIDENCE_DIR="${TIER2_DIR}/evidence/NAT-001"
SCRIPTS_DIR="${TIER2_DIR}/scripts"
ROOTFS_DIR="${TIER2_DIR}/rootfs"
CROSS_CC="${TIER2_DIR}/aarch64-linux-musl-cross/bin/aarch64-linux-musl-gcc"

mkdir -p "${EVIDENCE_DIR}"

BOOT_NUM=0
TOTAL_HITS=0
TOTAL_ITERS=0

while [ $BOOT_NUM -lt 10 ]; do
    echo "[run_nat001] Boot $((BOOT_NUM + 1))/10 starting..."

    # 1. Compile harness
    echo "[run_nat001] Compiling test_nat001.c..."
    ${CROSS_CC} -static -O0 -g -o "${ROOTFS_DIR}/harness" "${SCRIPTS_DIR}/test_nat001.c" -pthread -lrt
    if [ $? -ne 0 ]; then
        echo "[run_nat001] ERROR: Compilation failed"
        exit 1
    fi

    # 2. Package initramfs
    echo "[run_nat001] Packaging initramfs..."
    cd "${ROOTFS_DIR}"
    chmod +x init harness
    find . -print0 | cpio --null -ov --format=newc > "${TIER2_DIR}/initramfs.cpio" 2>/dev/null

    # 3. Launch QEMU with kasan=off nokaslr for NAT-001
    echo "[run_nat001] Launching QEMU (kasan=off nokaslr)..."
    cd "${TIER2_DIR}"
    KERNEL="${TIER2_DIR}/android/artifacts/Image"
    RAMDISK="${TIER2_DIR}/initramfs.cpio"
    CMDLINE="console=ttyAMA0 root=/dev/ram0 kasan=off nokaslr earlycon=pl011,0x09000000 printk.devkmsg=on rw"
    
    qemu-system-aarch64 \
        -M virt \
        -cpu cortex-a57 \
        -smp 2 \
        -m 2048 \
        -kernel "${KERNEL}" \
        -initrd "${RAMDISK}" \
        -append "${CMDLINE}" \
        -display none \
        -serial file:"${EVIDENCE_DIR}/qemu_boot${BOOT_NUM}.log" \
        -no-reboot &
    QEMU_PID=$!

    # 4. Wait for QEMU to be ready and run harness via serial console
    echo "[run_nat001] Waiting for harness completion (timeout 300s)..."
    TIMEOUT=300
    ELAPSED=0
    while [ $ELAPSED -lt $TIMEOUT ]; do
        if ! kill -0 $QEMU_PID 2>/dev/null; then
            echo "[run_nat001] QEMU exited unexpectedly"
            break
        fi
        # Check for completion marker in serial log
        if grep -q "NAT-001.*FINAL" "${EVIDENCE_DIR}/qemu_boot${BOOT_NUM}.log" 2>/dev/null; then
            echo "[run_nat001] Harness completed"
            break
        fi
        sleep 5
        ELAPSED=$((ELAPSED + 5))
    done

    if [ $ELAPSED -ge $TIMEOUT ]; then
        echo "[run_nat001] TIMEOUT: killing QEMU"
        kill $QEMU_PID 2>/dev/null || true
        pkill -f qemu-system-aarch64 2>/dev/null || true
        sleep 2
    fi

    # 5. Extract results from QEMU serial log
    LOG_FILE="${EVIDENCE_DIR}/qemu_boot${BOOT_NUM}.log"
    if grep -q "NAT-001.*FINAL" "$LOG_FILE"; then
        HITS=$(grep "NAT-001.*FINAL" "$LOG_FILE" | sed -n 's/.* \([0-9]*\) hits in \([0-9]*\) iterations.*/\1/p')
        ITERS=$(grep "NAT-001.*FINAL" "$LOG_FILE" | sed -n 's/.* \([0-9]*\) hits in \([0-9]*\) iterations.*/\2/p')
        if [ -n "$HITS" ] && [ -n "$ITERS" ]; then
            TOTAL_HITS=$((TOTAL_HITS + HITS))
            TOTAL_ITERS=$((TOTAL_ITERS + ITERS))
            echo "[run_nat001] Boot $((BOOT_NUM + 1)): $HITS hits in $ITERS iterations"
        fi
    else
        echo "[run_nat001] WARNING: No FINAL line found in log"
        # Dump last 50 lines for debugging
        tail -50 "$LOG_FILE" || true
    fi

    # 6. Clean up QEMU
    kill $QEMU_PID 2>/dev/null || true
    pkill -f qemu-system-aarch64 2>/dev/null || true
    sleep 1

    BOOT_NUM=$((BOOT_NUM + 1))
done

echo "[run_nat001] ALL BOOTS COMPLETE"
echo "[run_nat001] TOTAL: ${TOTAL_HITS} hits in ${TOTAL_ITERS} iterations"

# Write summary
cat > "${EVIDENCE_DIR}/NAT-001_SUMMARY.md" <<EOF
# NAT-001 Summary

## Configuration
- Kernel: linux-6.12.67 (Android 14 GKI, commit 7e35917775b8)
- Config: PREEMPT_VOLUNTARY, SLUB_CPU_PARTIAL, HZ=1000
- QEMU: virt, cortex-a57, 2 CPUs, 2GB RAM
- cmdline: kasan=off nokaslr

## Timing-Widening Techniques Applied
1. **False-sharing cache-line bouncing** (Thread C on CPU 0): 64-byte cache line hammered
2. **Slab prefill** (5000 eventpoll fds): fills kmalloc-192 cpu_partial lists
3. **Timer/IPI storm** (Thread D on CPU 1): 100 timerfds + sched_yield loop
4. **Multi-epitem topology**: outer epoll watches 2 inner epolls (epitem 1, epitem 2)

## Results
| Boot | Hits | Iterations | Cumulative Hit Rate |
|------|------|------------|---------------------|
EOF

for i in $(seq 1 $BOOT_NUM); do
    LOG="${EVIDENCE_DIR}/qemu_boot$((i-1)).log"
    if grep -q "NAT-001.*FINAL" "$LOG"; then
        H=$(grep "NAT-001.*FINAL" "$LOG" | sed -n 's/.* \([0-9]*\) hits in \([0-9]*\) iterations.*/\1/p')
        I=$(grep "NAT-001.*FINAL" "$LOG" | sed -n 's/.* \([0-9]*\) hits in \([0-9]*\) iterations.*/\2/p')
        echo "| $i | $H | $I | $(echo "scale=6; $H/$I*100" | bc -l 2>/dev/null || echo "N/A")% |" >> "${EVIDENCE_DIR}/NAT-001_SUMMARY.md"
    fi
done

cat >> "${EVIDENCE_DIR}/NAT-001_SUMMARY.md" <<EOF

## Total
- **Total Hits**: ${TOTAL_HITS}
- **Total Iterations**: ${TOTAL_ITERS}
- **Overall Hit Rate**: $(echo "scale=6; ${TOTAL_HITS}/${TOTAL_ITERS}*100" | bc -l 2>/dev/null || echo "N/A")%

## Wilson 95% Confidence Interval
$(if [ $TOTAL_ITERS -gt 0 ]; then
    python3 -c "
import math
hits = ${TOTAL_HITS}
total = ${TOTAL_ITERS}
if total > 0:
    p = hits / total
    z = 1.96
    denom = 1 + z*z/total
    centre = (p + z*z/(2*total)) / denom
    half = z * math.sqrt(p*(1-p)/total + z*z/(4*total*total)) / denom
    print(f'- **Lower**: {centre - half:.6f}')
    print(f'- **Upper**: {centre + half:.6f}')
"
fi)

## Conclusion
$(if [ ${TOTAL_HITS} -gt 0 ]; then echo "**RACE TRIGGERED NATURALLY** — Hit rate > 0 with timing widening"; else echo "**NO HITS** — Race not naturally winnable even with widening techniques (10,000 iterations)"; fi)
EOF

echo "[run_nat001] Summary written to ${EVIDENCE_DIR}/NAT-001_SUMMARY.md"