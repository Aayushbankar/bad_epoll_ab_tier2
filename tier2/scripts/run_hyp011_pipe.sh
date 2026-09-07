#!/bin/bash
# run_hyp011_pipe.sh — HYP-011: Pipe Control Arm Execution
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." &> /dev/null && pwd)"
TIER2_DIR="${PROJECT_ROOT}/tier2"
CROSS="${TIER2_DIR}/aarch64-linux-musl-cross/bin/aarch64-linux-musl-gcc"
EVIDENCE_DIR="${TIER2_DIR}/evidence/HYP-011"

mkdir -p "${EVIDENCE_DIR}"

echo "============================================================"
echo "HYP-011 Control Arm: Pipe Spray (H-c Prediction Check)"
echo "Kernel: GKI 6.1.23 (nokaslr, QEMU TCG)"
echo "============================================================"

# 1. Compile init with "pipe" argument
echo "[1/4] Preparing init..."
cat << 'INIT_EOF' > "${TIER2_DIR}/rootfs/init.c"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/reboot.h>

int main() {
    printf("[INIT] Starting init process...\n");
    pid_t pid = fork();
    if (pid == 0) {
        char *argv[] = { "/harness", "pipe", NULL };
        char *envp[] = { NULL };
        execve("/harness", argv, envp);
        perror("[INIT] execve failed");
        exit(1);
    }
    int status;
    waitpid(pid, &status, 0);
    printf("[INIT] Harness exited with status %d. Powering off.\n", status);
    reboot(RB_POWER_OFF);
    return 0;
}
INIT_EOF

${CROSS} -static -O2 -o "${TIER2_DIR}/rootfs/init" "${TIER2_DIR}/rootfs/init.c" \
    2>&1 || { echo "[!] init compile failed"; exit 1; }

# 2. Compile harness
echo "[2/4] Compiling test_hyp011_aar..."
${CROSS} -static -O0 -g -o "${TIER2_DIR}/rootfs/harness" "${TIER2_DIR}/scripts/test_hyp011_aar.c" \
    -pthread 2>&1 || { echo "[!] harness compile failed"; exit 1; }

# 3. Package initramfs
echo "[3/4] Packaging initramfs..."
cd "${TIER2_DIR}/rootfs"
chmod +x init harness
find . -print0 | cpio --null -ov --format=newc > "${TIER2_DIR}/initramfs.cpio" 2>/dev/null
cd "${TIER2_DIR}"

# 4. Launch QEMU with GDB stub
echo "[4/4] Launching QEMU with GDB stub on :1234..."
pkill -f qemu-system-aarch64 || true
sleep 1

DEBUG=1 ./scripts/run_qemu.sh > qemu_hyp011_pipe.log 2>&1 &
QEMU_PID=$!
echo "[*] QEMU launched with PID ${QEMU_PID}"
sleep 2

# 5. Run GDB script
echo "[*] Running GDB automation script..."
gdb -batch -q -x "${TIER2_DIR}/scripts/exp_hyp011_aar_gdb.py" "${TIER2_DIR}/android/artifacts/vmlinux" 2>&1 || true

# 6. Cleanup QEMU
echo "[*] Stopping QEMU..."
kill ${QEMU_PID} 2>/dev/null || true
pkill -f qemu-system-aarch64 2>/dev/null || true

if [ -f "${TIER2_DIR}/qemu_serial.log" ]; then
    cp "${TIER2_DIR}/qemu_serial.log" "${EVIDENCE_DIR}/HYP-011_control_pipe_serial.log" 2>/dev/null || true
fi
if [ -f "${EVIDENCE_DIR}/HYP-011_part1_raw_gdb.log" ]; then
    mv "${EVIDENCE_DIR}/HYP-011_part1_raw_gdb.log" "${EVIDENCE_DIR}/HYP-011_control_pipe_raw_gdb.log" 2>/dev/null || true
fi

# Restore default init.c
cat << 'DEF_INIT' > "${TIER2_DIR}/rootfs/init.c"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/reboot.h>

int main() {
    printf("[INIT] Starting init process...\n");
    pid_t pid = fork();
    if (pid == 0) {
        char *argv[] = { "/harness", NULL };
        char *envp[] = { NULL };
        execve("/harness", argv, envp);
        perror("[INIT] execve failed");
        exit(1);
    }
    int status;
    waitpid(pid, &status, 0);
    printf("[INIT] Harness exited with status %d. Powering off.\n", status);
    reboot(RB_POWER_OFF);
    return 0;
}
DEF_INIT

echo ""
echo "=== Pipe Control Arm Finished ==="
