# Verification Ledger

This document serves as the authoritative, machine-parseable Single Source of Truth (SSOT) mapping every verified research claim to its physical evidence, target kernel symbol, execution method, and raw log artifact.

---

## Verified Claims & Physical Evidence Matrix

| Verification ID | Date & Timestamp (UTC) | Claim / Fact Description | Target Symbol / Address | Verification Method | Raw Evidence File | Status |
|---|---|---|---|---|---|---|
| VER-009 | 2026-07-23 | Runtime observation of single-watch pipe_buffer cross-cache UAF on struct file->f_ep | __ep_remove | RUNTIME | tier2/evidence/EXP-006_raw_gdb.log | INCONCLUSIVE |
| VER-010 | 2026-07-24 | ~~Runtime observation of single-watch NULL stale-write triggering kernel panic in __mutex_lock_slowpath~~ | __list_add_valid | RUNTIME | INVALID (Protocol Violation) | **RETRACTED** — see VER-012 |
| VER-011 | 2026-07-24 | `struct epitem` is 120 bytes, allocated from `kmalloc-128` (NOT `kmalloc-192`). The stale write is `list_del_init(&epi->rdllink)` targeting offsets 24 and 32, not a NULL write to offset 160. | `struct epitem` | STATIC (GDB `ptype /o`) | tier2/evidence/2026-07-24/ep_remove_disassembly.txt | **VERIFIED** |
| VER-012 | 2026-07-24 | `__ep_remove` is called via `ep_eventpoll_release` → `ep_clear_and_put` → `ep_remove_safe` when closing `outer_epoll`. GDB breakpoint confirms entry with `ep=0xffffff807fa1e000`, `epi=0xffffff800438ef00`. Stale-write instructions are at `__ep_remove` offsets +284, +288, +292, +296. | `__ep_remove` @ `0xffffffc0083bce2c` | RUNTIME (GDB trace) | tier2/evidence/2026-07-24/gdb_uaf_trace_first_pass.log | **VERIFIED** |
| VER-013 | 2026-07-24 | `__mutex_lock_slowpath` fires during `snd_timer_user_ioctl` contention, but `wait_list.next` is self-pointing (normal, NOT corrupted). The `snd_timer_user` mutex is NOT the UAF target because `snd_timer_user` (~168 bytes) is in `kmalloc-192`, not `kmalloc-128` where the freed epitem lives. | `__mutex_lock_slowpath` @ `0xffffffc008f60948` | RUNTIME (GDB trace) | tier2/evidence/2026-07-24/gdb_uaf_trace_first_pass.log | **VERIFIED** (normal behavior, not corruption) |

## Retraction Log

| VER ID | Date Retracted | Reason |
|--------|---------------|--------|
| VER-010 | 2026-07-24 | Claim was "NULL stale-write at offset 160 consumed by mutex slowpath." Retracted because: (1) the stale write is `list_del_init` at offsets 24/32, not NULL at 160; (2) the UAF victim is `kmalloc-128` not `kmalloc-192`; (3) the mutex slowpath hit was normal contention, not corruption. Original task log `INVALID (Protocol Violation)` may not survive session boundaries. |

## Evolution Notes

| EVO ID | Date | Note |
|--------|------|------|
| EVO-005 | 2026-07-24 | Corrected slab cache from `kmalloc-192` to `kmalloc-128`. All prior "offset 160" analysis is invalid. The `snd_timer_user` open cannot reclaim the freed epitem because they are in different slab caches. A new `kmalloc-128` spray strategy is required. |
