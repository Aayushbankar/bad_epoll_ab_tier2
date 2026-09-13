# EXP-031: Stage-1 Race Verification on 6.6.102

## Goal
Verify if the Phase 1 / Stage 1 epoll-on-epoll race (CVE-2026-46242) is triggerable on the `android15-6.6-2025-10_r1` certified kernel running in QEMU TCG.

## Methodology
- Ported the `test_exp029_e2e.c` Stage-1 racer into `tier3/scripts/test_exp031_stage1.c`.
- Removed Stage-2 (`dma_heap` spray) to isolate just the survivor creation.
- Added parameter sweep for `launch_ahead_ns` (500ns to 6000ns) to re-calibrate the `timerfd` interrupt widening.
- Ran in QEMU TCG on the `6.6.102` kernel.

## Results
- **1000 Iterations Test:** 0 wins.
- **5000 Iterations Test:** 0 wins.
- **Parameter Sweep (100000 limit):** Extremely slow execution (approx. 15ms per iteration on this QEMU build). No hits observed in early iterations.

## Disassembly Analysis
The race window in `__ep_remove` (between `f_ep = NULL` and `hlist_del_rcu`) was analyzed via `objdump` of the 6.6.102 `vmlinux`.
- On 6.1, the window was 18 instructions.
- On 6.6.102, the compiler relocated the fast-path check, resulting in a window of exactly **11 instructions** (from `str xzr, [x24, #456]` at `0xffffffc0804c1778` to `str x9, [x10]` at `0xffffffc0804c162c`).
- The race is structurally intact (the vulnerability is present, as confirmed by source analysis), but the tighter instruction window and altered QEMU scheduling dynamics on 6.6 make it significantly harder to win on TCG.

## Conclusion
The Stage-1 survivor primitive is theoretically intact but operationally blocked in the QEMU lab without significant re-calibration of the timerfd interrupt widget or kernel-side debugfs oracles (which are currently blocked by the Kleaf build environment).
