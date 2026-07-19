# Architecture & Environment Decisions

## 1. Emulator Execution Strategy (x86_64 Host vs ARM64 Guest)
The standard Android SDK `emulator` tool deprecates and actively blocks booting `arm64-v8a` system images on `x86_64` hosts due to the extreme performance overhead of software translation. Attempting to launch the API 34 ARM64 AVD results in: `FATAL | Avd's CPU Architecture 'arm64' is not supported by the QEMU2 emulator on x86_64 host.`

**Decision:** We will bypass the Android `emulator` binary wrapper entirely. Instead, we will extract the `kernel-ranchu`, `ramdisk.img`, `system.img`, and `vendor.img` from the Google APIs ARM64 System Image and boot them directly using pure `qemu-system-aarch64`.
**Justification:** This guarantees a 100% ARM64 testing environment for vulnerability research (where GKI architectural differences like PAC and BTI are critical), while avoiding the arbitrary restrictions of the Android Studio toolchain.

## 2. Kernel Source of Truth
Instead of blindly checking out the latest `common-android14-6.1` from AOSP, we extracted the kernel build identifiers directly from the uncompressed `kernel-ranchu` binary that ships with the API 34 system image.

- **Extracted String:** `Linux version 6.1.23-android14-4-00257-g7e35917775b8-ab9964412 (build-user@build-host)`
- **Kernel Version:** 6.1.23
- **Commit Hash:** `7e35917775b8`
- **Google CI Build ID:** `9964412`

**Decision:** All source code syncs and `vmlinux` symbol downloads will exactly target commit `7e35917775b8` and Build ID `9964412` to ensure our offset maps and GDB interactions perfectly align with the executing kernel.

## 3. Exploit Portability & GKI Protections
The Tier 1 exploit targeted Fedora 6.12. The GKI 6.1 environment introduces:
- **KASLR**: Present, requires leak.
- **PAC (Pointer Authentication Codes)**: ARM64 hardware mitigation. We cannot blindly overwrite function pointers without valid signatures.
- **BTI (Branch Target Identification)**: Restricts JOP/ROP gadget discovery.

These will be mapped and analyzed in `KERNEL_MAP.md` and `VULNERABILITY_VALIDATION.md`.

## 4. Symbol Acquisition (vmlinux)
The exact Google CI artifacts for Build ID `9964412` (April 2023) have exceeded the public Google Cloud Storage retention period and return a `NoSuchKey` error when attempting to download `vmlinux` from `ci.android.com`.
**Decision:** We will synchronize the AOSP kernel source precisely to commit `7e35917775b8` and build it locally using the exact identical Clang toolchain (`17.0.0 r487747`). This will produce a 100% structurally identical `vmlinux` providing perfect GDB symbol mapping.
