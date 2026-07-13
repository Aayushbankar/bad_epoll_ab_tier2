qemu-system-x86_64 -kernel linux-6.12.67/arch/x86/boot/bzImage -initrd initramfs.cpio -append "console=ttyS0 quiet kaslr" -nographic -smp 2 -m 2G -cpu kvm64,+smep,+smap -display none > /tmp/qemu2.log 2>&1 &
QEMU_PID=$!
sleep 5
echo "cat /proc/kallsyms | grep init_task" > /tmp/pipe.in
kill $QEMU_PID
