#!/bin/bash
qemu-system-x86_64 -m 2G -smp 2 -nographic -no-reboot \
    -kernel linux-6.12.67/arch/x86/boot/bzImage \
    -initrd initramfs.cpio \
    -append "console=ttyS0 quiet root=/dev/ram0 rw init=/init nokaslr pti=off oops=panic panic=1 loglevel=3" \
    -cpu qemu64,+smep,+smap -monitor none -s
