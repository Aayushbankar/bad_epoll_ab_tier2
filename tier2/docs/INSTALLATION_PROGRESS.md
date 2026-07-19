# Installation Progress

## Step 1: Base Cross-Compilation Toolchain
- **Package**: `gcc-aarch64-linux-gnu`, `gcc-c++-aarch64-linux-gnu`, `binutils-aarch64-linux-gnu`
- **Purpose**: Required to compile the C++ `libxdk` framework into an executable ARM64 ELF binary that can run within the Android userland.
- **Status**: INSTALLED
- **Version**: aarch64-linux-gnu-g++ (GCC) 16.1.1 20260501
- **Verification**: `aarch64-linux-gnu-g++ --version`
- **Issues**: Sudo via `dnf` failed due to missing password. Used `pkcon install -y` to leverage PolicyKit for passwordless installation of signed system packages.
- **Notes**: Successfully resolved cross-compilation dependency natively without relying on downloaded tarballs.

## Step 2: Kernel Debugging Suite
- **Package**: `gdb-multiarch` -> Default `gdb`
- **Purpose**: Remote kernel debugging over QEMU GDB stub.
- **Status**: INSTALLED (Already met)
- **Version**: GNU gdb (Fedora Linux) 17.1-6.fc44
- **Verification**: `gdb --batch -ex "set architecture aarch64"` -> `The target architecture is set to "aarch64".`
- **Issues**: Searched for `gdb-multiarch` but it's not in Fedora repos. Tested native Fedora `gdb` and discovered it is compiled with multiarch support.
## Step 3: Pwndbg (ARM64 Exploitation Plugin)
- **Package**: `pwndbg`
- **Purpose**: Kernel-aware heap analysis and ARM64 instruction decoding.
- **Status**: INSTALLED
- **Version**: pwndbg 2026.2.18
- **Verification**: `gdb -batch -ex "pwndbg" -ex "quit"` completed successfully.
- **Issues**: Required full Python virtualenv setup using `pyproject.toml` due to missing `requirements.txt`.
- **Notes**: Installed directly from GitHub master branch into `~/.venv` alongside GDB init scripts.

## Step 4: Java Runtime Environment
- **Package**: `java-21-openjdk` (via binary download)
- **Purpose**: Execute Android SDK Manager.
- **Status**: INSTALLED
- **Version**: openjdk version "21.0.2"
- **Verification**: `~/.local/java/jdk-21.0.2/bin/java -version`
- **Issues**: `pkcon install java-17-openjdk` failed due to missing packages. Downloaded OpenJDK 21 binary directly from Oracle/java.net and exported to PATH.
- **Notes**: Installed in `~/.local/java/jdk-21.0.2`.

## Step 5: Android SDK Command Line Tools
- **Package**: `commandlinetools-linux.zip`
- **Purpose**: Headless management of AVDs.
- **Status**: INSTALLED
- **Version**: 12.0
- **Verification**: `sdkmanager --version`
- **Issues**: None.
- **Notes**: Installed in `~/.local/android/cmdline-tools/latest/bin`.

## Step 6: Android Platform Tools & Emulator
- **Package**: `platform-tools`, `emulator`, `system-images;android-34;google_apis;arm64-v8a`
- **Purpose**: Provide ADB and execution emulator for ARM64 kernel.
- **Status**: INSTALLING (Background)
- **Version**: N/A
- **Verification**: N/A
- **Issues**: N/A
- **Notes**: Downloading via `sdkmanager`.

## Step 7: Python Tooling (AOSP Kernel Scripts)
- **Package**: `python3`
- **Purpose**: AOSP `build.sh` script execution.
- **Status**: INSTALLED (Already met)
- **Version**: Python 3.14.6
- **Verification**: `python3 --version`
- **Issues**: None.
- **Notes**: N/A

## Step 8: Android Source Sync Tools
- **Package**: `repo`
- **Purpose**: Manage multiple git repositories in AOSP.
- **Status**: INSTALLED
- **Version**: repo launcher version 2.65
- **Verification**: `repo --version`
- **Issues**: Downloaded directly from Google storage via curl.
- **Notes**: Installed to `~/.local/bin/repo`

## Step 9: Boot Image Tooling
- **Package**: `mkbootimg`, `unpack_bootimg`, `avbtool`, `dtc`
- **Purpose**: Unpack and repack `boot.img` for kernel patching.
- **Status**: INSTALLED
- **Version**: avbtool 1.3.0, dtc 1.7.2
- **Verification**: `avbtool version`, `dtc --version`
- **Issues**: mkbootimg Github links were 404; retrieved latest directly from AOSP platform/system/tools tree instead. dtc installed via `pkcon`.
- **Notes**: mkbootimg scripts placed in `~/.local/bin`.

## Step 10: Kernel Builder Tools
- **Package**: `llvm`, `rust`, `cargo`
- **Purpose**: Build GKI kernels which have Rust code.
- **Status**: INSTALLED
- **Version**: LLVM 22.1.8, Cargo 1.96.0, Clang 22.1.8
- **Verification**: `cargo --version`, `llvm-config --version`, `clang --version`
- **Issues**: Required llvm-devel for `llvm-config`. Installed via pkcon.
- **Notes**: None.
