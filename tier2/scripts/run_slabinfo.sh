#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." &> /dev/null && pwd)"
TIER2_DIR="${PROJECT_ROOT}/tier2"

cd "$TIER2_DIR/rootfs" || exit 1
chmod +x init
find . -print0 | cpio --null -ov --format=newc > ../initramfs.cpio 2>/dev/null
cd "$TIER2_DIR"

./scripts/run_qemu.sh &
QEMU_PID=$!
sleep 5

nc localhost 1234 << 'GDB'
set pagination off
file ./android/artifacts/vmlinux
target remote :1234
c &
shell sleep 2
# Updated 2026-08-08: use relative path after repo separation (was absolute bad-epoll-lab path)
shell echo "/print_slabinfo" | nc localhost 4444 > evidence/slabinfo.txt
GDB
kill $QEMU_PID
