# REDIRECT: Single Source of Truth (SSOT)

> [!IMPORTANT]
> This document has been consolidated into the project Single Source of Truth (SSOT).
> For active milestone tracking and verified checklists, refer to:
> **[CURRENT_PROGRESS.md](CURRENT_PROGRESS.md)**

---

# Master Checklist (Archived Snapshot)

## Repository
- [x] Create `tier2-android-port` branch *(renamed to `main` during repo separation 2026-08-08)*
- [x] Establish Tier 2 directory structure
- [x] Create base documentation (Roadmap, Checklist, Log, Learning Map)

## Host Environment
- [ ] Verify Fedora toolchain packages (`aarch64-linux-gnu-gcc`, `clang`)
- [ ] Configure Python virtual environments for tooling (`angrop`, `pwntools`)
- [ ] Verify QEMU ARM64 virtualization support on host

## Android Environment
- [ ] Install Android Command Line Tools (`sdkmanager`)
- [ ] Install Platform Tools (`adb`, `fastboot`)
- [ ] Install Android Emulator
- [ ] Define precise AVD (Android Virtual Device) configuration for reproducibility

## Kernel Acquisition
- [ ] Identify AOSP branch containing vulnerable `CVE-2026-46242` logic
- [ ] Download raw `boot.img` and unpack
- [ ] Locate corresponding `vmlinux` with BTF/DWARF symbols
- [ ] Verify kernel version matches target scope exactly

## Toolchains & Debuggers
- [ ] Install `gdb-multiarch`
- [ ] Install `pwndbg` or `gef` configured for ARM64
- [ ] Install `binutils-aarch64-linux-gnu`
- [ ] Install `radare2` / `ghidra` (for static analysis)
- [ ] Install `dtc` (Device Tree Compiler)
- [ ] Install `apktool` and `unpackbootimg`

## Exploit Development (Phases)
- [ ] Phase 1: Cross-compile basic payload
- [ ] Phase 2: Trigger vulnerability deterministically
- [ ] Phase 3: Construct ARM64 primitive (Read/Write)
- [ ] Phase 4: Identify PAC/BTI bypass
- [ ] Phase 5: Achieve root shell

## Testing & Evidence
- [ ] Test payload on fresh emulator boot
- [ ] Capture kernel panic logs for failures
- [ ] Capture terminal recording of successful execution
- [ ] Document final architecture changes from Tier 1

## Documentation
- [ ] Maintain daily research log
- [ ] Map out all open questions
- [ ] Write final technical report
