# Progress Tracker

> Update this file after completing each step. AI agents should check this file to know the current state.

## Current Status: 🟢 Tier 1 — Not Started

---

## Tier 1: Linux VM via QEMU

### Environment Setup
- [ ] Install build dependencies (`build-essential`, `qemu-system-x86`, `busybox-static`, `cpio`, etc.)
- [ ] Download Linux kernel 6.12.67 LTS source
- [ ] Configure kernel for QEMU (`make defconfig` + enable `CONFIG_EPOLL`, `CONFIG_DEBUG_INFO`)
- [ ] Compile kernel (`make -j$(nproc)`)
- [ ] Create minimal rootfs with busybox (initramfs)
- [ ] Verify QEMU boots the kernel successfully

### Exploit Setup
- [ ] Clone `J-jaeyoung/security-research` (branch: `submit-cve-2026-46242`)
- [ ] Read and annotate `exploit.cpp` — understand each `@step` marker
- [ ] Identify and resolve `libxdk` dependency
- [ ] Compile exploit with `g++ -static -O2`
- [ ] Inject compiled exploit into initramfs

### Execution
- [ ] Boot QEMU with vulnerable kernel
- [ ] Run exploit inside VM
- [ ] Capture output (root shell, dmesg, timing)
- [ ] Save logs to `logs/tier1-run-*.log`
- [ ] Document what was learned

---

## Tier 2: Android Emulator

### Environment Setup
- [ ] Install Android NDK via sdkmanager
- [ ] Download or build GKI kernel 6.6+ for emulator
- [ ] Configure Android emulator to boot with custom kernel

### Exploit Adaptation
- [ ] Cross-compile exploit for x86_64-linux-android / aarch64-linux-android
- [ ] Resolve any Android-specific compilation issues
- [ ] Push exploit to emulator via `adb`

### Execution
- [ ] Run exploit on Android emulator
- [ ] Capture `logcat` and `dmesg` output
- [ ] Document differences from Tier 1

---

## Tier 3: SELinux Analysis

- [ ] After achieving uid=0, check `getenforce` and `id` output
- [ ] Check SELinux context: `cat /proc/self/attr/current`
- [ ] Document what can and cannot be done under confined uid=0
- [ ] Research if ROP chain can call `setenforce(0)`
- [ ] Write up the "reality check" section

---

## Article

- [ ] Draft introduction
- [ ] Write technical deep-dive section
- [ ] Write hands-on walkthrough section
- [ ] Write Android reality / SELinux section
- [ ] Write conclusion and impact section
- [ ] Internal review by supervisor
- [ ] Publish to Medium
- [ ] Publish to LinkedIn

---

## Log

| Date | Action | Notes |
|------|--------|-------|
| 2026-07-06 | Project initialized | Created repo structure and documentation |
