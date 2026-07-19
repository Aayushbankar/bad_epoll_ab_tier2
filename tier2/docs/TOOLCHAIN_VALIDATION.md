# Toolchain Validation Report

**Date**: 2026-07-15

| Tool | Status | Version | Notes |
| :--- | :--- | :--- | :--- |
| **Java** | Verified | OpenJDK 21.0.2 | Validated |
| **Android ADB** | Verified | 1.0.41 (37.0.0-14910828) | Validated |
| **Fastboot** | Verified | 37.0.0-14910828 | Validated |
| **Emulator** | Verified | 36.6.11.0 (QEMU 10.2.2-1) | *Requires explicit path invocation `~/.local/android/emulator/emulator`* |
| **Repo** | Verified | `~/.local/bin/repo` | Validated via background sync |
| **Bazel** | Pending | AOSP Hermetic | Waiting for `repo sync` completion |
| **LLVM / Clang** | Verified | 22.1.8 | System native |
| **Cross Compiler** | Verified | `aarch64-linux-gnu-gcc` 16.1.1 | System native |
| **QEMU (Standalone)**| Verified | 10.2.2-1 | `qemu-system-aarch64` native |
| **GDB (Multiarch)** | Verified | 17.1-6 | Native GDB supports cross-architecture. `gdb-multiarch` alias is unnecessary. |
| **pwndbg** | Verified | Loaded 199 commands | Verified directly within GDB invocation |
| **Python** | Verified | 3.14.6 | System native |
