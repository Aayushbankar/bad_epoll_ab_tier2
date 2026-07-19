# Knowledge & Learning Map

This document tracks the conceptual dependencies required to complete Tier 2, measuring current understanding against the target expertise needed for execution.

## Operating Systems & General Architecture
### Linux Kernel
- **Current understanding**: Proficient (Understands x86_64 SLUB allocators, virtual memory mapping, standard UAF triggers).
- **Target understanding**: Advanced (Need to understand differences in Android memory management, `init` vs Zygote).
- **Resources**: [Placeholder]
- **Notes**: [Placeholder]

### Android Kernel (GKI)
- **Current understanding**: Novice (Aware of GKI standardization).
- **Target understanding**: Expert (Must understand KMI, vendor hooks, and exact Android 14 kernel architecture).
- **Resources**: [Placeholder]
- **Notes**: [Placeholder]

### ARM64 Architecture
- **Current understanding**: Novice.
- **Target understanding**: Proficient (Instruction set, register conventions `$x0-$x30`, `$sp`, `$pc`, exception levels `EL0-EL3`).
- **Resources**: [Placeholder]
- **Notes**: [Placeholder]

## Exploit Mechanics
### Memory Management (SLUB on ARM64)
- **Current understanding**: Basic.
- **Target understanding**: Proficient (Alignment constraints, slab layout differences compared to x86_64).
- **Resources**: [Placeholder]
- **Notes**: [Placeholder]

### Return-Oriented / Jump-Oriented Programming (ROP/JOP)
- **Current understanding**: Proficient on x86_64.
- **Target understanding**: Proficient on ARM64 (Finding `br xN` or `blr xN` gadgets).
- **Resources**: [Placeholder]
- **Notes**: [Placeholder]

## Modern ARM64 Security Mitigations
### PAC (Pointer Authentication Codes)
- **Current understanding**: Conceptual (Cryptographic signing of pointers).
- **Target understanding**: Tactical (Understanding which registers are signed, how to forge signatures or avoid PAC-protected pointers entirely).
- **Resources**: [Placeholder]
- **Notes**: [Placeholder]

### BTI (Branch Target Identification)
- **Current understanding**: Novice.
- **Target understanding**: Tactical (Understanding `BTI c` / `BTI j` landing pads and their impact on JOP).
- **Resources**: [Placeholder]
- **Notes**: [Placeholder]

## Android Specific Security
### SELinux
- **Current understanding**: Conceptual.
- **Target understanding**: Tactical (Kernel-level structures, locating `selinux_enforcing` or `selinux_state`, and patching memory).
- **Resources**: [Placeholder]
- **Notes**: [Placeholder]

### Binder IPC
- **Current understanding**: Novice.
- **Target understanding**: Proficient (Optional but useful for interacting with privileged services post-exploitation).
- **Resources**: [Placeholder]
- **Notes**: [Placeholder]

## Environment Orchestration
### QEMU / Android Emulator
- **Current understanding**: Proficient with raw QEMU.
- **Target understanding**: Proficient with AVDs and Android-specific QEMU wrappers.
- **Resources**: [Placeholder]
- **Notes**: [Placeholder]

### ADB & Fastboot
- **Current understanding**: Basic.
- **Target understanding**: Proficient (Flashing boot images, pushing payloads, extracting logs).
- **Resources**: [Placeholder]
- **Notes**: [Placeholder]

### Kernel Debugging (GDB on ARM64)
- **Current understanding**: Proficient on x86_64.
- **Target understanding**: Proficient on ARM64 (Translating `info registers` and utilizing `pwndbg` ARM features).
- **Resources**: [Placeholder]
- **Notes**: [Placeholder]
