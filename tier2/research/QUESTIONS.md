# Research Questions & Unknowns

## General Android Kernel
- How does Android GKI differ from upstream mainline Linux?
- How are vendor modules loaded and how do they impact the memory layout?
- How does Android boot differ from standard Linux initramfs?
- How are vendor kernels distributed and packaged?
- How can a specific, vulnerable build be obtained or generated?

## Exploitation Mechanics (ARM64)
- Where is `commit_creds` located in the GKI memory layout?
- What changes exist in the `task_struct` on Android vs Tier 1?
- How is SELinux enforced within the kernel, and what structures need to be patched to disable it?
- How does PAC (Pointer Authentication Code) affect control flow hijacking?
- How does BTI (Branch Target Identification) affect JOP (Jump-Oriented Programming) chains?
- What are the `kmalloc` vs `kmem_cache` alignment discrepancies on ARM64 SLUB?

## Environment & Tooling
- How is a root shell cleanly spawned as an Android APK payload vs an ADB shell?
- What is the most reliable way to extract `vmlinux` from a raw Android device or emulator image?
