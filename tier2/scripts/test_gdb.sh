#!/bin/bash
# Launch QEMU with GDB stub for controlled race validation
# CVE-2026-46242 — Tier 2 controlled lifetime experiment
#
# This script:
# 1. Builds the initramfs with the trigger binary
# 2. Launches QEMU with -s (gdbstub on port 1234) and -S (frozen at start)
# 3. Waits for GDB to connect and control execution

set +e

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$REPO_ROOT"

# Rebuild initramfs
cp tier2/reproducers/cve_2026_46242_trigger tier2/rootfs/
cd tier2
scripts/build_rootfs.sh
cd "$REPO_ROOT"

pkill -9 qemu-system-aarch64 2>/dev/null
sleep 1

echo "[*] Starting QEMU with GDB stub on :1234 (frozen at start)"
echo "[*] Connect with: /usr/libexec/gdb -x tier2/scripts/gdb_race_commands.gdb"

QEMU_ARGS=(
    -M virt,mte=on
    -cpu max
    -smp 2
    -accel tcg,thread=multi
    -m 2048
    -kernel tier2/android/artifacts/Image
    -initrd tier2/initramfs.cpio
    -append "console=ttyAMA0 root=/dev/ram0 kasan=on nokaslr earlycon=pl011,0x09000000 printk.devkmsg=on rw"
    -nographic
    -no-reboot
    -gdb tcp::1234
)

qemu-system-aarch64 "${QEMU_ARGS[@]}" 2>&1 | tee /tmp/qemu_gdb_session.log &
echo $! > /tmp/qemu_gdb.pid
echo "[*] QEMU PID: $(cat /tmp/qemu_gdb.pid)"
echo "[*] Waiting for GDB stub to be ready..."
sleep 2
echo "[*] QEMU started. Now run the GDB script."
