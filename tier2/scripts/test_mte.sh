#!/bin/bash
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
qemu-system-aarch64 "${QEMU_ARGS[@]}" > /tmp/mte_test.log 2>&1 &
echo $! > /tmp/mte_test.pid
sleep 3
grep -i kasan /tmp/mte_test.log
kill $(cat /tmp/mte_test.pid)
