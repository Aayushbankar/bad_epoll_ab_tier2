#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
TIER2_DIR="$(cd "${SCRIPT_DIR}/.." &> /dev/null && pwd)"
cd "${TIER2_DIR}"

KERNEL="${TIER2_DIR}/android/artifacts/Image"
VMLINUX="${TIER2_DIR}/android/artifacts/vmlinux"
CC="${TIER2_DIR}/aarch64-linux-musl-cross/bin/aarch64-linux-musl-gcc"
NM="${TIER2_DIR}/aarch64-linux-musl-cross/bin/aarch64-linux-musl-nm"

# Compile NAT-005 harness without PIE
"$CC" -static -fno-pie -no-pie -O0 -g -o rootfs/harness scripts/test_nat005.c -pthread

# Find address of write() in userspace
WRITE_ADDR=$("$NM" rootfs/harness | grep " T write" | awk '{print $1}')
WRITE_ADDR="0x${WRITE_ADDR}"

# Compile init
"$CC" -static -O2 -o rootfs/init scripts/init_serial.c

# Package rootfs
cd rootfs
chmod +x init harness
find . -print0 | cpio --null -ov --format=newc > ../initramfs.cpio 2>/dev/null
cd "${TIER2_DIR}"

logfile_abs="${TIER2_DIR}/evidence/AND-002_raw_kaslr_on.log"
rm -f "$logfile_abs"
touch "$logfile_abs"

cat << GDB_EOF > /tmp/gdb_hook.py
import gdb
import time
import sys

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
GDB_EOF

# Run QEMU
qemu-system-aarch64 \
    -M virt \
    -cpu cortex-a57 \
    -smp 2 \
    -m 2048 \
    -kernel "$KERNEL" \
    -initrd initramfs.cpio \
    -append "console=ttyAMA0 root=/dev/ram0 kasan=off earlycon=pl011,0x09000000 printk.devkmsg=on isolcpus=1 nohz_full=1 rcu_nocbs=1 rw kaslr" \
    -display none \
    -serial null \
    -no-reboot \
    -nographic -s -S &
QEMU_PID=$!

# Run GDB
timeout 2400 gdb -batch -x /tmp/gdb_hook.py > /dev/null 2>&1 || true

kill $QEMU_PID 2>/dev/null || true
wait $QEMU_PID 2>/dev/null || true
