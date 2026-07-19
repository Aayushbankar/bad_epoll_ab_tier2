# Toolchain Installation Order

This document outlines the strict installation sequence required to provision the Fedora host for Android Tier 2 exploit development.

## Step 1: Base Cross-Compilation Toolchain
- **Package**: `gcc-aarch64-linux-gnu`, `g++-aarch64-linux-gnu`, `binutils-aarch64-linux-gnu`
- **Reason**: Required to compile the C++ `libxdk` framework into an executable ARM64 ELF binary that can run within the Android userland.
- **Dependencies**: `make`, `gcc`, `glibc-devel`
- **Verification command**: `aarch64-linux-gnu-g++ --version`
- **Expected output**: `aarch64-linux-gnu-g++ (GCC) 13.x.x ...`

## Step 2: Kernel Debugging Suite
- **Package**: `gdb-multiarch`
- **Reason**: Standard GDB on Fedora x86_64 cannot understand ARM64 registers or instructions. `multiarch` is required to attach to the QEMU/Emulator remote GDB stub.
- **Dependencies**: `python3`, `libncurses`
- **Verification command**: `gdb-multiarch --version`
- **Expected output**: `GNU gdb (GDB) ...`

## Step 3: Pwndbg (ARM64 Exploitation Plugin)
- **Package**: `pwndbg` (Cloned via git)
- **Reason**: Provides kernel-aware heap analysis (SLUB), KASLR bypass calculations, and ARM64 instruction decoding natively inside GDB.
- **Dependencies**: `gdb-multiarch`, `python3-pip`
- **Verification command**: `gdb-multiarch -ex "pwndbg" -ex "quit"`
- **Expected output**: `pwndbg: loaded ... commands.`

## Step 4: Java Runtime Environment
- **Package**: `java-17-openjdk` (or latest LTS)
- **Reason**: The Android SDK Manager (`sdkmanager`), which downloads the Emulator and system images, is a Java application.
- **Dependencies**: None
- **Verification command**: `java -version`
- **Expected output**: `openjdk version "17.x.x" ...`

## Step 5: Android SDK Command Line Tools
- **Package**: `commandlinetools-linux.zip` (Direct from Google)
- **Reason**: Headless management of Android Virtual Devices (AVDs) and SDK packages.
- **Dependencies**: `java-17-openjdk`, `unzip`
- **Verification command**: `sdkmanager --version`
- **Expected output**: `11.0 (or current version number)`

## Step 6: Android Platform Tools & Emulator
- **Package**: `platform-tools`, `emulator`, `system-images;android-34;google_apis;arm64-v8a` (via sdkmanager)
- **Reason**: `platform-tools` provides `adb` (Android Debug Bridge) to push payloads to the emulator. `emulator` runs the actual ARM64 kernel.
- **Dependencies**: Android SDK CMD Line Tools
- **Verification command**: `adb --version && emulator -accel-check`
- **Expected output**: `Android Debug Bridge version ...` followed by `accel: ...`

## Step 7: Clang / LLVM (Optional but Recommended)
- **Package**: `clang`, `llvm`
- **Reason**: Secondary compilation option. Modern Android kernels and eBPF payloads strictly rely on Clang.
- **Dependencies**: None
- **Verification command**: `clang --target=aarch64-linux-gnu --version`
- **Expected output**: `clang version ... Target: aarch64-unknown-linux-gnu`
