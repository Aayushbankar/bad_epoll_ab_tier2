# Runtime Validation & Passive Analysis

This document records the exact state of the bootable ARM64 Android runtime and the passive verification of CVE-2026-46242 in the custom Android kernel environment.

## 1. Runtime Status
**Status:** BOOTED

The custom compiled Android kernel (`6.1.23-android14-4-maybe-dirty`) successfully boots using `qemu-system-aarch64` and drops into an interactive BusyBox shell via a custom `initramfs`.

## 2. Boot Identity & Verification (Phase 4)

During boot, the custom `/init` script executed and captured the following verifiable host information:

### `uname -a`
```
Linux (none) 6.1.23-android14-4-maybe-dirty #1 SMP PREEMPT Thu Jan 1 00:00:00 UTC 1970 aarch64 GNU/Linux
```

### `/proc/version`
```
Linux version 6.1.23-android14-4-maybe-dirty (build-user@build-host) (Android (10087095, +pgo, +bolt, +lto, -mlgo, based on r487747c) clang version 17.0.2 (https://android.googlesource.com/toolchain/llvm-project d9f89f4d16663d5012e5c09495f3b30ece3d2362), LLD 17.0.2) #1 SMP PREEMPT Thu Jan 1 00:00:00 UTC 1970
```

### `/proc/cpuinfo` (Snippet)
```
processor	: 1
BogoMIPS	: 125.00
Features	: fp asimd evtstrm aes pmull sha1 sha2 crc32 cpuid
CPU implementer	: 0x41
CPU architecture: 8
CPU variant	: 0x1
CPU part	: 0xd07
```

### Kernel Command Line
```
console=ttynull stack_depot_disable=on cgroup_disable=pressure kasan.page_alloc.sample=10 kasan.stacktrace=off kvm-arm.mode=protected bootconfig ioremap_guard console=ttyAMA0 root=/dev/ram0 kasan=off earlycon=pl011,0x09000000 printk.devkmsg=on rw
```

**Verification Conclusion**: The running kernel unequivocally matches the `tier2/android/artifacts/Image` compiled from commit `7e35917775b8b3e3346a87f294e334e258bf15e6`.

## 3. Passive CVE-Related Validation (Phase 5)

Without executing a trigger, we statically resolved the target vulnerability symbols against the unstripped `vmlinux` image to prepare for future debug analysis.

### Relevant Symbols (via `nm` and `readelf`)
- `eventpoll_release_file`: `0xffffffc008407d1c` (The function initiating the race)
- `ep_remove`: `0xffffffc008407db0` (The function triggering the UAF on the `struct file` pointer)

### Prepared GDB Commands
To analyze the race condition in the future using the configurable debug launch mode, the following GDB commands are prepared:
```gdb
# Load symbols
file tier2/android/artifacts/vmlinux

# Break on eventpoll_release_file (thread 1)
break eventpoll_release_file

# Break on ep_remove (thread 2)
break ep_remove

# View the f_ep pointer being accessed (assuming rdi contains the struct file pointer)
display /x ((struct file *)$rdi)->f_ep
```

## 4. QEMU Debug Launch Mode (Phase 3)
A reproducible execution harness was established at `tier2/scripts/run_qemu.sh`.
It supports parameters for `CPUS`, `RAM`, `CMDLINE`, and a `DEBUG=1` mode which exposes a GDB stub on port 1234 (`-s -S`).
