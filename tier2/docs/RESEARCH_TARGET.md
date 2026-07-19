# Research Target & Lineage Analysis

## 1. Primary Research Target Evaluation

### Target Candidates
1. **Android Emulator (AVD)**: Easiest to spin up. Full GDB stub support (`-qemu -s`). Deterministic.
2. **Android Cuttlefish**: Highly customizable cloud-native Android environment, excellent for kernel development, but higher host overhead.
3. **Physical Pixel (e.g., Pixel 8)**: True hardware mitigations (MTE/PAC), but requires unlocking, flashing, and hardware serial debugging is incredibly difficult if a kernel panic occurs.
4. **Physical Vivo/Oppo/Redmi**: Vendor heavily forks the kernel. Bootloaders are often locked or difficult to unlock. Zero hardware debugging. Extremely high risk of bricking.
5. **Generic GKI Image**: Pure kernel, highly reproducible, but lacks the full Android userland (Zygote, Binder) needed to test true privilege escalation.

### Recommendation: Android Emulator (AVD) running a Generic System Image (GSI)
**Technical Justification**: Kernel exploitation research requires hundreds of crashes (Kernel Panics). Physical devices take minutes to reboot and require complex hardware UART setups to extract panic logs. The Android Emulator allows instantaneous snapshot restoration, exposes a GDB stub that allows halting the exact CPU instruction during a memory corruption, and runs standard ARM64 architecture. It provides the perfect balance of 100% debuggability and a realistic Android userland. Physical Pixel testing should only occur in Phase 6.

---

## 2. Android Version Selection

### Candidate Comparison
- **Android 13**: Established GKI 2.0. Good documentation. Highly mitigated.
- **Android 14**: Current stable baseline. Standardized 6.1 GKI kernel. Widespread source availability.
- **Android 15**: Bleeding edge. Kernels are shifting to 6.6. Documentation and emulator stability may fluctuate.
- **Android 16**: Developer preview. Kernels (6.12+) are heavily experimental. High engineering risk due to moving targets.

### Recommendation: Android 14 (Kernel 6.1 GKI)
**Technical Justification**: Android 14 mandates GKI 2.0 and standardizes heavily on the Linux 6.1 LTS branch. This provides an incredibly stable, heavily documented research target. Google publishes exact DWARF-compiled `vmlinux` artifacts for Android 14 GKI branches, meaning we don't have to guess offsets. Android 15/16 introduce volatility in the SLUB allocator that would unnecessarily complicate the porting of the Tier 1 exploit.

---

## 3. Kernel Lineage & Portability (Linux 6.12.67 vs Android 14 GKI 6.1)

The Tier 1 exploit was developed and proven against **Linux 6.12.67**. The recommended Android target uses **Android 14 GKI (Linux 6.1.x)**.

### What Remained (Ancestry)
- **`epoll` Subsystem**: The core event poll architecture and wait-queue structures (`epitem`) are fundamentally identical between 6.1 and 6.12. The `close-vs-close` race condition (CVE-2026-46242) fundamentally affects both lineages.
- **VFS (Virtual File System)**: File descriptor handling, `f_op` structs, and `poll` abstractions remain architecturally compatible.

### What Changed (Obstacles & Differences)
- **SLUB Allocator**: Linux 6.12 introduced several optimizations and freelist hardening features that are NOT present in 6.1. Conversely, Android GKI compiles the kernel with specific `CONFIG_SLAB_FREELIST_RANDOM` and `CONFIG_SLAB_FREELIST_HARDENED` options that may differ from default Fedora 6.12 builds. The cross-cache spray timing will shift.
- **`task_struct` Layout**: Android heavily modifies process accounting. The offset to `cred` pointers within `task_struct` will be entirely different on GKI 6.1 compared to Fedora 6.12.
- **Security Mitigations**:
  - **PAC (Pointer Authentication)**: Android 14 ARM64 kernels compiled by Google enforce PAC on function pointers. The Tier 1 exploit overwrote `f_op->poll` (a function pointer). On ARM64 GKI, hijacking `f_op->poll` will immediately trigger a PAC signature fault and panic the kernel.
  - **BTI (Branch Target Identification)**: JOP gadgets (jumping to `br x1`) require landing on a `BTI c` or `BTI j` instruction. The gadget discovery pipeline used in Tier 1 will yield 95% unusable gadgets on Android.
  - **KFI (Kernel Control Flow Integrity)**: Android uses Clang CFI, adding strict type-checking to indirect calls.

### Conclusion on Portability
The core vulnerability (the UAF trigger and the resulting Arbitrary Address Read/Write) is highly portable. The escalation payload (the JOP/ROP chain and `f_op->poll` hijack) is completely broken and must be redesigned from zero to accommodate PAC and CFI.
