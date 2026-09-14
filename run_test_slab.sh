#!/bin/bash
cp test_slab tier2/rootfs/init
cd tier2/rootfs && find . -print0 | cpio --null -ov --format=newc > ../initramfs.cpio && cd ../..
./tier2/scripts/run_qemu.sh > qemu_slab.log 2>&1 &
Q=$!
sleep 15
kill $Q || true
pkill -f qemu-system-aarch64
cat qemu_slab.log | grep "/sys/kernel/slab"
