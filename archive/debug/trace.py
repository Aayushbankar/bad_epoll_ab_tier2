import pexpect
import time

print("[*] Starting QEMU...")
qemu = pexpect.spawn('./start_qemu.sh')

print("[*] Starting GDB...")
gdb = pexpect.spawn('gdb-multiarch -q --nx -ex "set debuginfod enabled off" -ex "set pagination off"')
gdb.expect(r'\(gdb\)')

print("[*] Waiting for QEMU to boot...")
time.sleep(3)

print("[*] Connecting GDB to QEMU...")
gdb.sendline('target remote localhost:1234')
gdb.expect(r'\(gdb\)')

# Step 1: Break after rop_build_privesc returns
gdb.sendline('hbreak *0x40dd0a')
gdb.expect(r'\(gdb\)')

print("[*] Running exploit in QEMU...")
# The initramfs automatically drops to shell, but we need to run /bin/exploit
# Let's send the command to QEMU console.
# Since start_qemu.sh routes console to qemu_output.log in background, we can't easily interact via expect on qemu.
# Let's just create a modified initramfs that auto-runs the exploit on boot!
