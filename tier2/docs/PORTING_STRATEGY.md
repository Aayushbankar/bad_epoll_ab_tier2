# Android Tier 2 Porting Strategy

## Current Linux Exploit Architecture (Tier 1)
The Tier 1 exploit relies on a `close-vs-close` epoll race condition to trigger a UAF, allowing a cross-cache spray to establish Arbitrary Address Read (AAR) and Write (AAW) primitives. Execution control is hijacked by overwriting the `f_op->poll` function pointer of a controlled file descriptor. A generated offset database (`target_db.kxdb`) maps a JOP pivot into a ROP chain that executes `commit_creds(init_task)` and successfully returns to userland (`iretq`).

## Android Architecture Differences
Android enforces vastly different security boundaries and architectural paradigms on ARM64 compared to x86_64 Fedora:
- **Architecture**: ARM64 uses `X0-X30` registers, Link Registers (`LR`), and Exception Levels (`EL0`-`EL3`) rather than x86_64 Rings. 
- **Mitigations**: PAC cryptographically signs function pointers, neutralizing the Tier 1 `f_op->poll` hijack. BTI neutralizes traditional JOP padding. Clang CFI prevents jumping to arbitrary signatures.
- **Data Structures**: The `task_struct` and `cred` structures contain Android-specific telemetry and Binder states.
- **SELinux**: Unlike Fedora (where gaining `UID 0` is sufficient), Android relies heavily on SELinux. Even as root, an exploit must patch the kernel's `selinux_state` to disable enforcement, or hijack a privileged Binder context (like `system_server`).

## Expected Reusable Components
- **The Core Vulnerability (Trigger)**: The `epoll` threading and race-timing logic should remain mostly intact.
- **The `libxdk` Primitive Abstraction**: The `MemoryData` classes mapping out the AAR/AAW read/write abstractions are architecture-agnostic.
- **Task Search Logic**: Scanning physical memory for `init_task` via known heuristics is highly reusable, provided the signatures are updated.

## Components That Must Be Redesigned
- **Execution Hijack Vector**: Overwriting `f_op->poll` is impossible due to PAC. A data-only attack (e.g., overwriting `core_pattern` or `modprobe_path`, though these are likely restricted on Android) or finding a non-PAC-signed function pointer is required.
- **Gadget Database**: The SQLite `kxdb` pipeline must be entirely re-engineered to look for ARM64 instructions (`LDR`, `STR`, `BR`, `RET`) and must filter out gadgets lacking `BTI` landing pads.
- **Privilege Escalation Payload**: Must include SELinux neutralization logic.
- **Userland Return**: `iretq` stack alignment logic must be replaced with ARM64 `ERET` logic or a clean thread exit (`do_exit`).

## Unknowns
- Exactly how the GKI 6.1 SLUB allocator aligns the specific `epitem` structure compared to Linux 6.12.
- The precise timing variance introduced by the Android Emulator's translated CPU ticks vs a native kernel.
- Whether a sufficiently powerful data-only attack surface exists to bypass the need for control-flow hijacking entirely.

## Research Priorities
1. Establish a stable AVD emulator with a vulnerable GKI 6.1 kernel.
2. Compile and test the raw UAF trigger to verify the panic signature.
3. Decompile the target GKI `vmlinux` to locate `selinux_state` and verify PAC implementation on file operations.

## Engineering Risks
- **High Risk**: Hardware mitigations (PAC/BTI) might prove unbreakable on this specific GKI build using the current primitive, forcing a pivot to a pure data-only approach.
- **Medium Risk**: Android userland might crash instantly upon the exploit's return to EL0 if the `SP_EL0` stack pointer is not perfectly restored.

## Success Metrics
- A reproducible python/shell script that boots the emulator, pushes the binary via ADB, and executes it.
- GDB successfully halting at the UAF trigger point.
- The exploit successfully leaking the ARM64 KASLR base.
- A final root shell (`UID 0`) spawned with `getenforce` returning `Permissive`.
