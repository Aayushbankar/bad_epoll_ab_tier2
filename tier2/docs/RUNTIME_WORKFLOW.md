# Android ARM64 Runtime Workflow

This runbook contains exact copy-paste commands to operate the Tier 2 custom kernel research environment.

## 1. Rebuild the Initramfs (Userspace Artifact Injection)
When you modify or compile a new test binary, it must be injected into the rootfs and repacked.
```bash
# 1. Place your compiled binary into tier2/rootfs/
cp my_exploit tier2/rootfs/

# 2. Rebuild the initramfs
./tier2/scripts/build_rootfs.sh
```

## 2. Start QEMU normally
This will boot the custom kernel and drop you into an interactive `/ #` shell.
```bash
./tier2/scripts/run_qemu.sh
```

## 3. Start QEMU with Verbose Logging
To view full kernel boot logs on the console:
```bash
CMDLINE="console=ttyAMA0 root=/dev/ram0 kasan=off earlycon=pl011,0x09000000 printk.devkmsg=on rw debug ignore_loglevel" ./tier2/scripts/run_qemu.sh
```

## 4. Start QEMU in GDB-Wait Mode
To pause the kernel at startup and wait for a debugger on port 1234:
```bash
DEBUG=1 ./tier2/scripts/run_qemu.sh
```
In a separate terminal, attach GDB:
```bash
gdb-multiarch tier2/android/artifacts/vmlinux -ex "target remote localhost:1234"
```

## 5. Stop the Guest
Inside the QEMU console, use the QEMU escape sequence to terminate the VM:
Press `Ctrl-a`, release, then press `x`.
Alternatively, run `poweroff -f` from the `/ #` guest shell.

## 6. Verify Running Kernel Identity
Inside the guest shell, run:
```bash
cat /proc/version
```
Look for `6.1.23-android14-4-maybe-dirty ... based on r487747c`.

## 7. Collect Runtime Evidence
To capture the execution details into a timestamped evidence vault, use the evidence collection script (which handles launching QEMU and capturing its output automatically):
```bash
./tier2/scripts/collect_runtime_evidence.sh
```
