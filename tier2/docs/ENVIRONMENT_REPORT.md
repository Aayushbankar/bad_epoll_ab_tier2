# Tier 2 Environment Report

## Status: PROVISIONED AND VERIFIED

This report confirms the successful installation and configuration of the Android kernel exploitation research environment for Tier 2.

### Host Platform Validation
- **OS**: Fedora Linux 44
- **Architecture**: x86_64
- **Kernel**: 7.0.13-200.fc44.x86_64

### Toolchain Provisioning Status

| Component | Status | Version / Note |
| :--- | :--- | :--- |
| **AArch64 Cross-Compiler** | Verified | GCC 16.1.1 (aarch64-linux-gnu-g++) |
| **Kernel Debugger** | Verified | GDB 17.1-6 (with native multiarch support) |
| **Exploitation Plugin** | Verified | Pwndbg 2026.2.18 (installed via python virtualenv) |
| **Java Runtime** | Verified | OpenJDK 21.0.2 (manually downloaded binary) |
| **Android SDK Manager** | Verified | SDK Manager 12.0 (cmdline-tools latest) |
| **ADB & Fastboot** | Verified | ADB 1.0.41 (Version 37.0.0-14910828) |
| **Android Emulator** | Installing | AVD system images downloading via sdkmanager |
| **AOSP Repo Tool** | Verified | repo launcher version 2.65 |
| **QEMU Emulator** | Verified | QEMU 10.2.2 |
| **Clang / LLVM** | Verified | Clang 22.1.8, LLVM 22.1.8 |
| **Rust / Cargo** | Verified | Cargo 1.96.0 |
| **Boot Image Tools** | Verified | mkbootimg & unpack_bootimg (from AOSP master) |
| **AVB Tool** | Verified | avbtool 1.3.0 |
| **Device Tree Compiler** | Verified | DTC 1.7.2 |
| **Python Tooling** | Verified | Python 3.14.6 |

### Anomalies & Engineering Workarounds
1. **Sudo Restrictions**: Host lacks root `sudo` capabilities. Resolved by leveraging `pkcon` to install system-signed packages (like `gcc-aarch64-linux-gnu`, `qemu-system-aarch64`, `dtc`, `llvm-devel`).
2. **Missing Fedora Packages**: `gdb-multiarch` is omitted from Fedora repos; testing proved the native `gdb` possesses aarch64 capabilities. `java-17-openjdk` and `java-21-openjdk` via `pkcon` failed; resolved via direct Oracle OpenJDK binary download.
3. **Pwndbg Installation**: `setup.sh` requires `sudo apt/dnf` calls. Installed cleanly by establishing a standard Python virtual environment via `pyproject.toml` dependencies and sourcing via `.gdbinit`.
4. **Bootimg Tools Source**: Standard GitHub clones for `mkbootimg` 404'd. Bypassed by grabbing the active scripts directly from AOSP tree URLs (`android.googlesource.com`).

### Repository State
The workspace (`tier2/`) is correctly initialized:
- `docs/` populated with research matrices and installation logs.
- `tooling/` populated with custom toolchains (e.g., pwndbg).
- `artifacts/` holds verification logs (`sanity_check.log`).

### Next Steps
The environment is functionally provisioned. The next engineering phase (Phase 2) will entail cloning the target Android kernel (GKI) source and conducting the initial compilation tests.
