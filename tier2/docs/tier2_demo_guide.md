# Tier 2 Progress Summary & Demo Guide (CVE-2026-46242 - Android ARM64)

## 1. Executive Summary of Progress
For Tier 2, we have successfully established the fundamental Android **ARM64** research environment targeting **Android 14 GKI** (Generic Kernel Image), pinned specifically to kernel source commit `7e35917775b8`.

**Key Achievements:**
- Overcame Android hermetic toolchain sync issues (e.g., Clang symlinks) and build system errors (`__isoc23_strtol` linker error in `resolve_btfids`).
- Successfully compiled the custom kernel, producing `vmlinux` (with debug symbols), the bootable `Image`, and `System.map`.
- Pivoted away from the unsupported Android x86_64 emulator and successfully booted the ARM64 kernel using `qemu-system-aarch64` and a custom-built minimal BusyBox `initramfs`.
- The environment reliably drops to an interactive `/ #` root shell, allowing execution of arbitrary cross-compiled C programs.

**Current Status (LEVEL 2 — RUNTIME CONFIRMED):**
The baseline infrastructure for injecting and running test binaries is fully operational. However, **the CVE has NOT yet been conclusively reproduced on Android.** We determined that our initial timing-based assumptions were false positives. We do not yet have direct kernel-side evidence (like a KASAN report or OOPS) proving the Use-After-Free triggers in this specific Android layout. The project is currently focused strictly on passive observation and verifiable evidence generation using our existing test harnesses.

---

## 2. Manual Run Steps (Demo Execution)

Use the following exact commands to demonstrate the current working state of the Tier 2 Android ARM64 VM.

### Step A: Navigate to the Project Root
Ensure you are in the root directory before running the scripts:
```bash
# Updated 2026-08-08: use repo-relative path after repo separation
cd "$(git rev-parse --show-toplevel)"
```

### Step B: (Optional) Rebuild the Initramfs
If the mentor asks how userspace binaries are injected, explain that we cross-compile them with `aarch64-linux-gnu-gcc` and package them into the initramfs. If you need to rebuild it:
```bash
./tier2/scripts/build_rootfs.sh
```

### Step C: Boot the QEMU Environment
Launch the ARM64 Android kernel using the freshly built initramfs:
```bash
./tier2/scripts/run_qemu.sh
```
*Note: This will output QEMU startup logs followed by Linux kernel boot messages, eventually dropping you into a `/ #` BusyBox shell.*

### Step D: Execute the Test Harness (Inside QEMU)
Once the guest boots, run the injected test binary to demonstrate execution capabilities:
```bash
/trigger
```
*(If the binary is named differently in the rootfs, adjust accordingly. This will run the race condition attempt.)*

### Step E: Check for Kernel Faults (Inside QEMU)
To prove we are actively monitoring for memory safety violations, check the kernel ring buffer for any faults:
```bash
dmesg | grep -iE 'kasan|bug|oops|panic|epoll'
```
*Note: We currently expect this to be clean (or not show a UAF panic), which correctly reflects our current status of searching for verifiable evidence.*

### Step F: Cleanly Exit QEMU
When the demo is over, stop the QEMU guest and return to the host terminal:
- Press `Ctrl-a`, release both keys, then press `x`.
