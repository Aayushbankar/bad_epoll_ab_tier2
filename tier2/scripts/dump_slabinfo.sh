#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." &> /dev/null && pwd)"
TIER2_DIR="${PROJECT_ROOT}/tier2"

cd "$PROJECT_ROOT"
cat << 'INIT' > tier2/rootfs/init
#!/bin/sh
mount -t proc none /proc
cat /proc/slabinfo > /slabinfo.txt
cat /slabinfo.txt
sleep 1
INIT
chmod +x tier2/rootfs/init

echo "[*] Packaging rootfs..."
cd "$TIER2_DIR/rootfs" || exit 1
find * -print0 | cpio --null -ov --format=newc > ../initramfs.cpio 2>/dev/null
cd "$TIER2_DIR"

CMDLINE="console=ttyAMA0 root=/dev/ram0 kasan=on nokaslr earlycon=pl011,0x09000000 printk.devkmsg=on rw" 
qemu-system-aarch64 -M virt -cpu cortex-a57 -smp 2 -m 2048 \
    -kernel android/artifacts/Image \
    -initrd initramfs.cpio \
    -append "$CMDLINE" -nographic -no-reboot > /tmp/qemu_slabinfo.log 2>&1 &
QEMU_PID=$!
sleep 5
kill $QEMU_PID 2>/dev/null || true
cat /tmp/qemu_slabinfo.log | grep -E "filp|kmalloc|slabinfo"
