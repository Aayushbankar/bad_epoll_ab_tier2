# Command Reference

This document lists the validated, existing commands for interacting with the Tier 2 laboratory environment. Do not use experimental or undocumented exploit commands.

## Initramfs & Artifact Preparation
Rebuild the initial RAM disk (initramfs) incorporating any files currently in `tier2/rootfs/`:
```bash
./tier2/scripts/build_rootfs.sh
```

## QEMU Runtime Execution
Launch the Android ARM64 kernel in QEMU normally (drops to a `/ #` root shell):
```bash
./tier2/scripts/run_qemu.sh
```

Launch QEMU with verbose kernel boot logging and `kasan=off`:
```bash
CMDLINE="console=ttyAMA0 root=/dev/ram0 kasan=off earlycon=pl011,0x09000000 printk.devkmsg=on rw debug ignore_loglevel" ./tier2/scripts/run_qemu.sh
```

Launch QEMU paused, waiting for a GDB debugger connection on port 1234:
```bash
DEBUG=1 ./tier2/scripts/run_qemu.sh
```

## Evidence Collection
Automatically launch QEMU, capture the kernel boot output, generate artifact hashes, and store the results in a timestamped evidence vault (`tier2/evidence/`):
```bash
./tier2/scripts/collect_runtime_evidence.sh
```
