# Installation Checklist

Ensure the following tools and dependencies are installed on the host environment before commencing Tier 2 execution.

## Android Ecosystem
- [ ] Android SDK
- [ ] Android Platform Tools (adb, fastboot)
- [ ] Android Emulator
- [ ] AOSP build dependencies (if compiling custom GKI)

## Compilers & Toolchains
- [ ] LLVM / Clang (configured for ARM64)
- [ ] Cross compilers (`aarch64-linux-gnu-gcc`, `aarch64-linux-gnu-g++`)
- [ ] binutils (`aarch64-linux-gnu-objdump`, etc.)

## Debugging & Triage
- [ ] QEMU (ARM64 support: `qemu-system-aarch64`)
- [ ] gdb-multiarch
- [ ] pwndbg / GEF (configured with ARM64 support)
- [ ] radare2
- [ ] ghidra
- [ ] IDA Free (optional, for decompilation)

## Firmware & Image Utilities
- [ ] apktool (for payload injection)
- [ ] avbtool (Android Verified Boot utility)
- [ ] mkbootimg (building boot images)
- [ ] unpackbootimg (extracting kernels and ramdisks)
- [ ] dtc (Device Tree Compiler)
- [ ] abootimg / magiskboot (optional, for boot image patching)
