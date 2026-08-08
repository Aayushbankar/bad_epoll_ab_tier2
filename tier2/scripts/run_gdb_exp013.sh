#!/bin/bash
# Updated 2026-08-08: use relative path after repo separation (was absolute bad-epoll-lab path)
cd "$(dirname "$0")/.."

echo "[*] Starting QEMU in background..."
nohup qemu-system-aarch64 -machine virt,mte=off -cpu cortex-a57 -smp 2 -m 1G -kernel android/artifacts/Image -initrd initramfs.cpio -append "console=ttyAMA0 oops=panic panic=-1 slub_debug=FZ" -nographic -no-reboot -s -S > scripts/qemu.log 2>&1 &
QEMU_PID=$!
echo $QEMU_PID > scripts/emulator.pid

echo "[*] Waiting 2 seconds for QEMU to bind to port 1234..."
sleep 2

echo "[*] Starting GDB..."
gdb -q -batch android/artifacts/vmlinux -x scripts/gdb_exp013_eventpoll_uaf.py > evidence/EXP-013_raw_gdb.log 2>&1

echo "[*] Killing QEMU..."
kill $QEMU_PID 2>/dev/null
