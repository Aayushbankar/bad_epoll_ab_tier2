#!/bin/bash
cp tier2/reproducers/cve_2026_46242_trigger tier2/rootfs/
cd tier2
scripts/build_rootfs.sh
cd ..
QEMU_ARGS=(
    -M virt,mte=on
    -cpu max
    -smp 2
    -m 2048
    -kernel tier2/android/artifacts/Image
    -initrd tier2/initramfs.cpio
    -append "console=ttyAMA0 root=/dev/ram0 kasan=on nokaslr earlycon=pl011,0x09000000 printk.devkmsg=on rw"
    -nographic
    -no-reboot
)
qemu-system-aarch64 "${QEMU_ARGS[@]}" > /tmp/repro_test.log 2>&1 &
echo $! > /tmp/repro_test.pid
