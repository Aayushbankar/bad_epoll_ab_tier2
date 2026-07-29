# Verification Ledger

This document serves as the authoritative, machine-parseable Single Source of Truth (SSOT) mapping every verified research claim to its physical evidence, target kernel symbol, execution method, and raw log artifact.

---

## Verified Claims & Physical Evidence Matrix

| Verification ID | Date & Timestamp (UTC) | Claim / Fact Description | Target Symbol / Address | Verification Method | Raw Evidence File | Status |
|---|---|---|---|---|---|---|
| VER-009 | 2026-07-23 | Runtime observation of single-watch pipe_buffer cross-cache UAF on struct file->f_ep | __ep_remove | RUNTIME | tier2/evidence/EXP-006_raw_gdb.log | INCONCLUSIVE |
| VER-010 | 2026-07-24 | ~~Runtime observation of single-watch NULL stale-write triggering kernel panic in __mutex_lock_slowpath~~ | __list_add_valid | RUNTIME | INVALID (Protocol Violation) | **RETRACTED** — see VER-012 |
| VER-011 | 2026-07-24 | `struct epitem` is 120 bytes, allocated from the dedicated `eventpoll_epi` slab cache (NOT `kmalloc-192`). The cache is created with `SLAB_HWCACHE_ALIGN|SLAB_PANIC|SLAB_ACCOUNT` at `eventpoll.c:2555`. The stale write is `list_del_init(&epi->rdllink)` targeting offsets 24 and 32, not a NULL write to offset 160. | `struct epitem` | STATIC (GDB `ptype /o`, kernel source) | tier2/evidence/2026-07-24/ep_remove_disassembly.txt | **VERIFIED** |
| VER-012 | 2026-07-24 | `__ep_remove` is called via `ep_eventpoll_release` → `ep_clear_and_put` → `ep_remove_safe` when closing `outer_epoll`. GDB breakpoint confirms entry with `ep=0xffffff807fa1e000`, `epi=0xffffff800438ef00`. Stale-write instructions are at `__ep_remove` offsets +284, +288, +292, +296. | `__ep_remove` @ `0xffffffc0083bce2c` | RUNTIME (GDB trace) | tier2/evidence/2026-07-24/gdb_uaf_trace_first_pass.log | **VERIFIED** |
| VER-013 | 2026-07-24 | `__mutex_lock_slowpath` fires during `snd_timer_user_ioctl` contention, but `wait_list.next` is self-pointing (normal, NOT corrupted). The `snd_timer_user` mutex is NOT the UAF target because `snd_timer_user` (~168 bytes) is in `kmalloc-192`, not the dedicated `eventpoll_epi` cache where the freed epitem lives. | `__mutex_lock_slowpath` @ `0xffffffc008f60948` | RUNTIME (GDB trace) | tier2/evidence/2026-07-24/gdb_uaf_trace_first_pass.log | **VERIFIED** (normal behavior, not corruption) |

## Retraction Log

| VER ID | Date Retracted | Reason |
|--------|---------------|--------|
| VER-010 | 2026-07-24 | Claim was "NULL stale-write at offset 160 consumed by mutex slowpath." Retracted because: (1) the stale write is `list_del_init` at offsets 24/32, not NULL at 160; (2) the UAF victim is in dedicated `eventpoll_epi` cache, not `kmalloc-192`; (3) the mutex slowpath hit was normal contention, not corruption. Original task log `INVALID (Protocol Violation)` may not survive session boundaries. |

## Evolution Notes

| EVO ID | Date | Note |
|--------|------|------|
| EVO-005 | 2026-07-24 | Corrected slab cache from `kmalloc-192` to dedicated `eventpoll_epi` cache (120 bytes, `SLAB_HWCACHE_ALIGN|SLAB_ACCOUNT`). All prior "offset 160" analysis is invalid. The `snd_timer_user` open cannot reclaim the freed epitem because they are in different slab caches. Same-cache reclaim via `epoll_ctl(EPOLL_CTL_ADD)` is the viable strategy. |
| EVO-006 | 2026-07-27 | Confirmed `eventpoll_epi` is a truly isolated dedicated cache. `CONFIG_MEMCG` is disabled (`.config` line: `# CONFIG_MEMCG is not set`), so `kmalloc-cg-128` does not exist. The cache's `SLAB_ACCOUNT` flag prevents merging into `kmalloc-128` (which lacks `SLAB_ACCOUNT`). Cross-cache reclaim from generic kmalloc caches is NOT possible. Only same-cache reclaim (allocating new `epitem` objects via `epoll_ctl`) can reclaim the freed slot. Source: `eventpoll.c:2555` (`kmem_cache_create`), `slab_common.c:173-215` (`find_mergeable`). |
| VER-014 | 2026-07-27 | Verified `eventpoll_epi` slab cache (used for `struct epitem`) is created with `SLAB_ACCOUNT` (eventpoll.c:2555). In `mm/slab_common.c:51/200`, `SLAB_ACCOUNT` is part of `SLAB_MERGE_SAME`, preventing merge with standard `kmalloc-128` (which lacks this flag). Cache is completely isolated. | `eventpoll_epi` | STATIC (Source code audit) | third_party/linux-6.12.67/fs/eventpoll.c, third_party/linux-6.12.67/mm/slab_common.c | **VERIFIED** |
| VER-015 | 2026-07-27 | Same-cache reclaim via `epoll_ctl(EPOLL_CTL_ADD)` successfully occupies the freed `epitem` slot. Freed `epitem` address (e.g., `0xffffff80043cb400`) perfectly matches the address of a newly allocated `epitem` during spray. | `struct epitem` | RUNTIME (GDB trace) | tier2/evidence/EXP-007_raw_gdb.log | **VERIFIED** |
| VER-016 | 2026-07-28 | EXP-008 Timing Analysis: The `list_del_init` corrupting write on `epi->rdllink` executes *before* `call_rcu(epi)` in `__ep_remove`. Therefore, the memory is corrupted before it is freed. When reclaimed via `epoll_ctl(EPOLL_CTL_ADD)`, the newly allocated `struct epitem` has its `rdllink` unavoidably overwritten by `INIT_LIST_HEAD` during initialization (`str x0, [x24, #0x18]; str x0, [x24, #0x20]`), destroying the `LIST_POISON` values. This makes the same-cache `list_del_init` path a structural dead-end regardless of allocation flags. | `__ep_remove` | RUNTIME (GDB trace) | tier2/evidence/EXP-008_raw_gdb.log | **VERIFIED** |
| VER-017 | 2026-07-29 | Reclaimed exact file pointer `0xffffff800233de00` via `open()` spraying (EXP-009). | `struct file` | RUNTIME (GDB trace) | tier2/evidence/EXP-009_final.log | **VERIFIED** |
| VER-018 | 2026-07-29 | EXP-010: `filp` cache (for `struct file`) is created with `SLAB_TYPESAFE_BY_RCU`, making it unmergeable (`slab_common.c:500`). `pipe_buffer` is allocated via `kcalloc` (generic `kmalloc-cg`). Additionally, `pipe_buffer` allocation size (40 bytes * power of 2) mathematically misses the 256-byte size class. Cross-cache reclaim from `pipe_buffer` to `struct file` is a structural dead end. | `filp_cachep` / `pipe_buffer` | STATIC (Source code audit) | tier2/evidence/EXP-010_RESULTS.md | **VERIFIED** |
| VER-019 | 2026-07-29 | EXP-011: Type-confusion on the reclaimed `struct file` via `ep_item_poll` is fundamentally unreachable. Any thread triggering the UAF race in `__ep_remove` must hold `ep->mtx`. Concurrently, any attempt to interact with the stale `epitem` (via `epoll_wait` or `EPOLL_CTL_MOD`) also blocks on `ep->mtx`. When the racing thread drops `ep->mtx`, the `epitem` has already been fully unlinked (from `rdllist` and `rbr`) and scheduled for RCU destruction. Thus, userspace cannot access the stale `epitem` to poll the reclaimed file. | `__ep_remove` | STATIC/TRACE | tier2/evidence/EXP-011_RESULTS.md | **VERIFIED** |
