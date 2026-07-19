# Environment Manifest

*This document serves as the canonical record of the research environment. It should be updated as soon as the Tier 2 toolchain is provisioned.*

## Host Machine
- **OS**: Fedora Linux 44 (Workstation Edition)
- **Kernel**: Linux 7.0.13-200.fc44.x86_64
- **CPU**: Intel(R) Core(TM) i5-8350U CPU @ 1.70GHz
- **RAM**: 7.6Gi Total (5.0Gi Used, 2.6Gi Available)
- **Storage**: 233G Total (114G Used, 119G Available on `/dev/nvme0n1p3`)

## Installed Packages
- **GCC Cross Compiler**: gcc (GCC) 16.1.1 20260515 (Red Hat 16.1.1-2)
- **Clang/LLVM**: clang version 22.1.8 (Fedora 22.1.8-1.fc44)
- **GDB Multiarch**: [Missing]
- **Python**: Python 3.14.6
- **Rust**: rustc 1.96.0
- **Cargo**: cargo 1.96.0

## Android Ecosystem
- **Java**: [Missing]
- **Android SDK Version**: [Missing]
- **ADB Version**: [Missing]
- **Fastboot Version**: [Missing]
- **Emulator Version**: [Missing]
- **QEMU (ARM64)**: [Missing]

## Target Environment (GKI / Emulator)
- **Device Model**: [E.g., Pixel 6 / Generic AVD]
- **Android Version**: [E.g., Android 14]
- **Kernel Version**: [E.g., 6.1.25-android14]
- **Security Patch Level**: [Date]

## Connected Devices
*(List of physical devices if used, or persistent AVD instances)*
- `[Serial Number / Emulator Port]` - `[Device Name]`
