# PoC Portability Assessment (CVE-2026-46242)

This document assesses the portability of the existing `kernelctf` reference exploit (`third_party/security-research/pocs/linux/kernelctf/CVE-2026-46242_lts_cos/original_exploit/lts-6.12.67/exploit.c`) to our custom Android ARM64 6.1.23 environment.

## Overview
- **Target Kernel:** LTS 6.12.67 (x86_64)
- **Vulnerability:** `ep_remove` Use-After-Free

## Analytical Breakdown

| Component | Original assumption | Target Android ARM64 status | Evidence |
|---|---|---|---|
| Race Trigger | `close()` and `epoll_ctl()` execute concurrently on x86_64 SMP. | Compatible. ARM64 SMP handles VFS/epoll locking identically. | `fs/eventpoll.c` source code matches upstream core logic. |
| Timing Oracle | Precision cycle counting using x86 `rdtsc` to measure race. | Compatible via adaptation. Requires `cntvct_el0`. | The PoC contains a macro `arm_cntvct()` using `mrs %0, cntvct_el0`. |
| Allocator | Target object (`epitem`) is served by generic `kmalloc-128`. | Incompatible. Android 6.1 GKI uses a dedicated `eventpoll_epi` cache. | `pahole` inspection and `fs/eventpoll.c` `kmem_cache_create("eventpoll_epi")`. |
| Cross-Cache | Overlapping freed `epitem` with `msg_msg` allocations works. | Incompatible/Requires new primitive. Needs cross-cache grooming. | SLUB freelist randomization and `kasan_hw_tags` block trivial overlaps. |
| Mitigations | KASAN disabled, no CFI/PAC/BTI, no SCS. | Incompatible. GKI 6.1 mandates CFI, PAC, SCS, and MTE (HW_TAGS). | Kernel config (`build.config.aarch64`) mandates these security features. |
| Payload | x86_64 ROP chain to `commit_creds(prepare_kernel_cred)`. | Incompatible. ARM64 assembly; PAC/SCS defeats standard ROP. | `exploit.c` contains hardcoded `swapgs`/`iretq` and x86 gadgets. |

## Conclusion
The original research PoC is highly specific to x86_64 and generic `kmalloc` slabs. To test LEVEL 3, we must cleanly excise the generic Linux behavior (the VFS race trigger and the timing oracle) and discard all allocator/payload components.
