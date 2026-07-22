# REDIRECT: Single Source of Truth (SSOT)

> [!IMPORTANT]
> This document has been consolidated into the project Single Source of Truth (SSOT).
> For current project status, phase tracking, and blockers, refer to:
> **[CURRENT_PROGRESS.md](file:///mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/docs/CURRENT_PROGRESS.md)**

---

# Canonical Project State (Archived Snapshot)

**Current Phase**: Phase 3 (Runtime Validation & Vulnerability Reproduction)
**Current Branch**: `tier2-android-port`
**Current Objective**: Establish a bootable ARM64 Android runtime using the custom kernel Image and design a trigger-only reproducer for the CVE-2026-46242 race condition (without exploit payload).

---

### Status Tracking
- **Repository Status**: Synced and patched (resolve_btfids Makefile fixed)
- **Last Completed Milestone**: Kernel Compilation & Source Analysis, QEMU ARM64 boot with BusyBox
- **Current Milestone**: Trigger-only Vulnerability Validation
- **Next Milestone**: Data-Only Exploitability Assessment
- **Build Status**: COMPLETE. (Kernel `6.1.23-android14-4-maybe-dirty`)
- **Reverse Engineering Readiness**: COMPLETE (struct layouts, offsets, mitigations analyzed)

---

### Blockers & Debt
- **Known Blockers**: NONE.
- **Engineering Debt**: Need to verify whether KASAN HW_TAGS prevents initial race trigger and if `kasan=off` works correctly in the emulator.
- **Research Debt**: Exploitability requires verifying cross-cache primitive and bypassing CFI/PAC.

---

### Knowledge Base
- **Verified Facts**: 
  - Host OS: Fedora (x86_64)
  - Target Kernel: `7e35917775b8` (Android 14 GKI)
  - Custom kernel built successfully (`tier2/android/artifacts/{vmlinux,Image,System.map}`).
  - `epitem` uses a dedicated `eventpoll_epi` slab cache (120 bytes, SLAB_HWCACHE_ALIGN).
- **Current Assumptions**: 
  - Raw `qemu-system-aarch64` successfully boots the custom Android 14 GKI `Image` with a BusyBox initramfs, providing a sufficient environment for vulnerability validation without needing the full Android Emulator.
- **Important Artifacts**: 
  - `tier2/android/artifacts/vmlinux`
  - `tier2/android/artifacts/Image`
  - `tier2/android/artifacts/System.map`
- **Important Documentation**: 
  - `tier2/docs/cve_2026_46242_analysis.md`
  - `tier2/docs/KERNEL_RESEARCH_DB.md`
  - `tier2/docs/RUNTIME_VALIDATION_PLAN.md` (To be created)

---

**Next Session Starting Point**: Validating KASAN configuration, establishing GDB connection to QEMU, and designing a trigger-only minimal reproducer.
