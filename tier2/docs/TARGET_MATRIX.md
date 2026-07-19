# Target Evaluation Matrix

| Target | Architecture | Kernel | Bootloader | Root Access | Debugging | Source Available | Recommended |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Android Emulator (AVD)** | ARM64 (Virtual) | GKI (v6.1+) | Unlocked | Yes (su) | Yes (GDB stubs) | Yes (AOSP) | **YES (Primary)** |
| **Android Cuttlefish** | ARM64 / x86_64 | GKI | Unlocked | Yes | Yes | Yes (AOSP) | Secondary |
| **Physical Pixel 8** | ARM64 (Tensor) | GKI | Unlockable | Magisk | KASAN/UART (Hard) | Yes (AOSP/Vendor)| No (Phase 2 only) |
| **Physical Vivo/Oppo** | ARM64 (MTK/QC) | Vendor Fork | Locked | No | Impossible | Partial | No |
| **Physical Redmi** | ARM64 | Vendor Fork | Unlockable | Magisk | UART (Hard) | Partial | No |
| **Generic GKI Image** | ARM64 | Pure GKI | N/A | N/A | Excellent | Full | Yes (For testing) |
