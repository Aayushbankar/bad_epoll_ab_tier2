#!/bin/bash
# run_hyp012.sh — HYP-012 Execution Launcher
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASE_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$BASE_DIR"

mkdir -p evidence/HYP-012

echo "[*] Killing existing QEMU processes..."
pkill -f qemu-system-aarch64 || true
sleep 1

echo "[*] Launching QEMU with DEBUG=1..."
DEBUG=1 ./scripts/run_qemu.sh > evidence/HYP-012/HYP-012_serial.log 2>&1 &
QEMU_PID=$!
sleep 2

echo "[*] Launching GDB automation script exp_hyp012_gdb.py..."
gdb -batch -q -x scripts/exp_hyp012_gdb.py android/artifacts/vmlinux || true

echo "[*] GDB completed. Waiting for QEMU to terminate..."
wait $QEMU_PID 2>/dev/null || true

echo "========================================================"
echo "=== HYP-012 Serial Log Summary ==="
echo "========================================================"
grep -E "HYP-012|MARKER|SUCCESS|PRIVILEGE|getuid|ROOT|AAR|PART" evidence/HYP-012/HYP-012_serial.log || tail -n 40 evidence/HYP-012/HYP-012_serial.log

echo "========================================================"
echo "=== HYP-012 GDB Log Summary ==="
echo "========================================================"
cat evidence/HYP-012/HYP-012_raw_gdb.log || true
