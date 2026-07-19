# Toolchain & Package Dependencies

This document outlines the required engineering toolchain for Android Kernel Privilege Escalation research, categorized by current availability.

## 1. Package Status on Host (Fedora)

### Already Installed
- `gcc` (16.1.1) - Required for cross-compilation fallback and local tool builds.
- `clang` (22.1.8) - Native C/C++ compilation.
- `python` (3.14.6) - Essential for debugging scripts, exploit wrappers, and binary analysis frameworks (`angrop`, `pwntools`).
- `rustc` / `cargo` (1.96.0) - For modern Rust-based reverse engineering tools.
- `git` - Source control.

### Missing
- `java` (OpenJDK) - Required by Android Command Line Tools (`sdkmanager`).
- `android-tools` / Platform Tools - Contains `adb` and `fastboot`.
- `qemu-system-aarch64` - Essential for emulating ARM64 if avoiding the heavy Android Emulator, or for nested kernel boots.
- `llvm-config` / `llvm` suite - Crucial for building custom eBPF payloads or analyzing clang-compiled kernel objects.
- `gdb-multiarch` - The sole interactive debugger capable of remote-debugging an ARM64 kernel from an x86_64 host.
- `binutils-aarch64-linux-gnu` - For `objdump` and `readelf` against ARM64 targets.

### Optional
- Android Studio - Heavy IDE; entirely optional as headless `sdkmanager` handles all AVD creation.
- `ghidra` / `radare2` - Helpful for static binary analysis of vendor blobs, but `gdb-multiarch` handles runtime.

---

## 2. Android Kernel Development Toolchain

| Tool / Dependency | Purpose & Role in Workflow |
| :--- | :--- |
| **Android SDK / cmdline-tools** | Manages the downloading of emulator images, platform-tools, and NDK. Required to construct a headless reproducible environment. |
| **Platform Tools (`adb`, `fastboot`)** | The bridge to the Android userland. Required to push the exploit binary to `/data/local/tmp/`, execute it, and extract kernel panic logs (`dmesg`). |
| **Android Emulator** | The primary execution target. Avoids bricking physical devices during kernel crashes. Offers GDB stubs (`-qemu -s -S`) for debugging. |
| **AOSP Dependencies** | Tools like `make`, `bison`, `flex`, `libssl-dev` needed if we decide to recompile the GKI kernel from source to inject print statements or verify mitigations. |
| **LLVM / Clang (AOSP Prebuilts)** | Android kernels (GKI) are strictly compiled with specific versions of Android LLVM. Using the host Clang often breaks kernel builds due to strict LTO or PAC compiler flags. |
| **Cross Compilers** | `aarch64-linux-gnu-gcc/g++` is needed to compile the actual C++ exploit (`exploit.cpp` + `libxdk`) into an ARM64 ELF binary that runs on Android. |
| **mkbootimg / unpack_bootimg** | Required to unpack a downloaded `boot.img`, extract the `vmlinux` binary for gadget discovery, or repack a custom kernel with an insecure ramdisk. |
| **avbtool** | Android Verified Boot tool. Needed to disable AVB (dm-verity) when flashing custom, debug-enabled kernels to a physical device or emulator. |
| **dtc (Device Tree Compiler)** | Used to decompile `.dtb` files within the boot image to understand hardware memory layouts or patch out security constraints before boot. |
| **repo tool** | Google's wrapper around git. Absolutely necessary for pulling the massive, multi-repository AOSP or Android Common Kernel (ACK) source trees. |
| **gdb-multiarch** | Standard host GDB cannot interpret ARM64 registers or instructions. This tool connects to the emulator's GDB stub to set hardware breakpoints in Ring 0. |
| **pwndbg** | A GDB plugin that provides kernel-aware heap inspection (SLUB structures), register highlighting, and ARM64 instruction decoding during exploitation. |
