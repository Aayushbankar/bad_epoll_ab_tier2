# Tier 2 Overview: Android Kernel Privilege Escalation

## Tier 2 Goals
The primary goal of Tier 2 is to successfully port the `CVE-2026-46242` Bad Epoll exploit, which has been verified on a local Linux x86_64 environment, to an Android ARM64 Generic Kernel Image (GKI) environment, achieving deterministic `UID 0` (root) execution and bypassing vendor-specific or standard Android security boundaries.

## Expected Milestones
1. **Environment Provisioning**: Establish an ARM64 Android emulator environment running a vulnerable GKI build with root shell and debugging access enabled.
2. **Architecture Adaptation**: Migrate memory structures, offsets, and timing expectations from x86_64 to ARM64.
3. **Primitive Reconstruction**: Achieve arbitrary read/write (AAR/AAW) against the ARM64 SLAB allocator.
4. **Mitigation Bypass**: Develop strategies to bypass ARM64-specific exploit mitigations, explicitly Pointer Authentication Codes (PAC) and Branch Target Identification (BTI).
5. **Control Flow Hijack**: Redirect kernel execution safely to an escalated payload.
6. **Privilege Escalation**: Successfully patch task credentials (`commit_creds`) or SELinux enforcing structures to obtain unconfined root access.
7. **Post-Exploitation Stability**: Ensure clean resumption of kernel threads without crashing the Android userland or triggering watchdog panics.

## Scope
- **In Scope**: Generic Kernel Image (GKI) manipulation, SLUB allocator grooming on ARM64, bypassing PAC/BTI, escaping SELinux context from within the kernel, adapting `libxdk` framework for ARM64 payloads.
- **Out of Scope**: Chrome/Browser sandbox escapes (starting assumption is arbitrary code execution from an untrusted app context), hardware-level fault injection, breaking TrustZone/TEE.

## Assumptions
- The vulnerability (`CVE-2026-46242`) exists and is reachable in the target GKI branch.
- An untrusted userland execution context is already established (e.g., adb shell or malicious APK).
- The host machine (Fedora) is capable of cross-compiling ARM64 payloads and orchestrating the Android Emulator.

## Success Criteria
- Deterministic triggering of the UAF vulnerability on an Android Emulator.
- Successful extraction of the kernel base address (KASLR bypass).
- Execution of a privilege escalation payload leading to a root shell (`UID 0`).
- Preservation of system stability (no kernel panics upon returning to user space).
- Comprehensive documentation and reproducibility of the exploit pipeline.

## Key Documentation & Index Ledger Navigation

| Navigation Category | Authoritative Document | Purpose |
|---|---|---|
| **Project Progress** | [CURRENT_PROGRESS.md](file:///mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/docs/CURRENT_PROGRESS.md) | Single Source of Truth for milestone status |
| **Verification Ledger** | [VERIFICATION_LEDGER.md](file:///mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/docs/VERIFICATION_LEDGER.md) | Machine-parseable matrix mapping claims to raw evidence |
| **Experiment Index** | [EXPERIMENT_INDEX.md](file:///mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/docs/EXPERIMENT_INDEX.md) | Registry of reproducers, GDB scripts, and shell runners |
| **Knowledge Evolution** | [KNOWLEDGE_EVOLUTION.md](file:///mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/docs/KNOWLEDGE_EVOLUTION.md) | Historical tracking of assumptions, discoveries, and lessons |
| **Documentation Map** | [DOCUMENTATION_MAP.md](file:///mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/docs/DOCUMENTATION_MAP.md) | Complete 15-domain architectural sitemap |

