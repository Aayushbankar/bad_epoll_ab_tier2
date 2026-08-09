#!/bin/bash
# run_and002_clean.sh — AND-002: KASLR Impact on Race Reliability
# Runs the NAT-005 calibrated harness under KASLR-off and KASLR-on configurations.
# Captures output via GDB hardware breakpoints on userspace `write()`.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
TIER2_DIR="$(cd "${SCRIPT_DIR}/.." &> /dev/null && pwd)"
cd "${TIER2_DIR}"

KERNEL="${TIER2_DIR}/android/artifacts/Image"
VMLINUX="${TIER2_DIR}/android/artifacts/vmlinux"
CC="${TIER2_DIR}/aarch64-linux-musl-cross/bin/aarch64-linux-musl-gcc"
NM="${TIER2_DIR}/aarch64-linux-musl-cross/bin/aarch64-linux-musl-nm"

mkdir -p evidence

# Compile NAT-005 harness without PIE so userspace addresses are fixed
echo "[AND-002] Compiling test_nat005.c harness (non-PIE)..."
"$CC" -static -fno-pie -no-pie -O0 -g -o rootfs/harness scripts/test_nat005.c -pthread

# Find address of write() in userspace
WRITE_ADDR=$("$NM" rootfs/harness | grep " T write" | awk '{print $1}')
WRITE_ADDR="0x${WRITE_ADDR}"
echo "[AND-002] Userspace write() address: $WRITE_ADDR"

# Compile init
echo "[AND-002] Compiling init_serial.c..."
"$CC" -static -O2 -o rootfs/init scripts/init_serial.c

# Package rootfs
echo "[AND-002] Packaging rootfs..."
cd rootfs
chmod +x init harness
find . -print0 | cpio --null -ov --format=newc > ../initramfs.cpio 2>/dev/null
cd "${TIER2_DIR}"

run_qemu_with_gdb() {
    local label="$1"
    local cmdline="$2"
    local logfile="$3"
    local timeout_secs="${4:-1800}"  # 30 min default
    local logfile_abs="${TIER2_DIR}/${logfile}"

    echo "[AND-002] === RUN: $label ==="
    echo "[AND-002] Cmdline: $cmdline"
    echo "[AND-002] Output:  $logfile"
    rm -f "$logfile_abs"
    touch "$logfile_abs"

    # Create GDB script
    cat << EOF > /tmp/gdb_hook.py
import gdb
import time
import sys

# We open in unbuffered mode if possible or just flush
log_file = open("${logfile_abs}", "w", buffering=1)

class WriteHook(gdb.Breakpoint):
    def stop(self):
        try:
            fd = int(gdb.parse_and_eval("\$x0"))
            if fd == 1 or fd == 2:
                buf = int(gdb.parse_and_eval("\$x1"))
                count = int(gdb.parse_and_eval("\$x2"))
                data = gdb.selected_inferior().read_memory(buf, count).tobytes()
                s = data.decode('utf-8', 'replace')
                sys.stdout.write(s)
                sys.stdout.flush()
                log_file.write(s)
                log_file.flush()
        except Exception as e:
            pass
        return False

for i in range(15):
    try:
        gdb.execute("target remote :1234")
        break
    except:
        time.sleep(1)

try:
    bp = WriteHook("*${WRITE_ADDR}", type=gdb.BP_HARDWARE_BREAKPOINT, internal=False)
except Exception as e:
    print(f"Error setting hbreak: {e}")

gdb.execute("continue")
gdb.execute("quit")
EOF

    # Run QEMU stopped
    qemu-system-aarch64 \
        -M virt \
        -cpu cortex-a57 \
        -smp 2 \
        -m 2048 \
        -kernel "$KERNEL" \
        -initrd initramfs.cpio \
        -append "$cmdline" \
        -display none \
        -serial null \
        -no-reboot \
        -nographic -s -S &
    QEMU_PID=$!

    # Run GDB
    timeout "$timeout_secs" gdb -batch -x /tmp/gdb_hook.py > /dev/null 2>&1 || true
    
    kill $QEMU_PID 2>/dev/null || true
    wait $QEMU_PID 2>/dev/null || true

    echo "[AND-002] QEMU exited for $label"
    if [ -f "$logfile_abs" ]; then
        echo "[AND-002] Log size: $(wc -c < "$logfile_abs") bytes"
        echo "[AND-002] Last 5 lines:"
        tail -5 "$logfile_abs" || true
    else
        echo "[AND-002] WARNING: No log file produced!"
    fi
    echo ""
}

COMMON_ARGS="root=/dev/ram0 kasan=off earlycon=pl011,0x09000000 printk.devkmsg=on isolcpus=1 nohz_full=1 rcu_nocbs=1 rw"

echo ""
echo "================================================================"
echo " AND-002: KASLR Impact on Race Hit Rate"
echo " Using NAT-005 calibrated harness (isolcpus + cache eviction)"
echo " Same binary, same iteration count, KASLR on vs off"
echo "================================================================"
echo ""

# RUN 1: KASLR OFF (baseline, matching NAT-005 config)
run_qemu_with_gdb_skip_first \
    "KASLR OFF (baseline)" \
    "console=ttyAMA0 ${COMMON_ARGS} nokaslr" \
    "evidence/AND-002_raw_kaslr_off.log" \
    600

# RUN 2: KASLR ON
run_qemu_with_gdb_skip_first \
    "KASLR ON" \
    "console=ttyAMA0 ${COMMON_ARGS} kaslr" \
    "evidence/AND-002_raw_kaslr_on.log" \
    600

echo "[AND-002] Both runs complete."
echo "[AND-002] Evidence files:"
ls -la evidence/AND-002_raw_kaslr_*.log 2>/dev/null || echo "(none found)"

