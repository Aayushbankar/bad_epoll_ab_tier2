qemu-system-x86_64 -kernel linux-6.12.67/arch/x86/boot/bzImage -initrd initramfs.cpio -append "console=ttyS0 quiet" -nographic -smp 2 -m 2G -cpu kvm64,+smep,+smap > /tmp/qemu.log 2>&1 &
QEMU_PID=$!
sleep 10
kill $QEMU_PID
