# Dead Ends Register

> **Status**: PERMANENT — Paths conclusively disproven, never to be revisited
> **Rule**: A path is dead only when evidence makes it structurally impossible
> **Format**: Each entry must cite the exact VER/EXP that killed it

---

## Exploitation Chains (Fully Disproven)

| ID | Path | Why Dead | Killing Evidence | Date |
|----|------|----------|------------------|------|
| DE-001 | **Chain 0**: `percpu_counter_dec` dereferences freed `ep->user` for controlled crash | `percpu_counter_dec` operates on OUTER epoll's `user`, not freed INNER eventpoll. `ep` parameter in `__ep_remove` is always outer. | VER-028 (EXP-019) — GDB trace shows outer epoll valid, no crash | 2026-08-01 |
| DE-002 | **Chain 1**: Dual-watch topology leaks kernel pointer via `msgrcv` readback | Multi-epitem condition FALSE → `WRITE_ONCE(f_ep, NULL)` doesn't execute → lockless bypass impossible → inner_epoll never freed before `hlist_del_rcu` → write to LIVE memory | VER-033 (EXP-024) — Hardware watchpoint shows `ep_free_inner_seen=False` at write time | 2026-08-02 |
| DE-003 | **Chain 2**: Arbitrary decrement via fake `user_struct` at `ep->user` offset 136 | `ep` parameter is OUTER epoll; GDB overwrite of `outer_epoll->user` failed to redirect decrement; `fbc = root_user+8` | VER-031 (EXP-023b) — Breakpoint at `percpu_counter_add_batch` shows outer epoll's user | 2026-08-01 |
| DE-004 | **Chain 3**: Full LPE via modprobe_path/cred corruption | Depends on DE-001/002/003 primitives which are dead | VER-028/031/033 | 2026-08-02 |

---

## UAF Target Objects (Disproven)

| ID | Path | Why Dead | Killing Evidence | Date |
|----|------|----------|------------------|------|
| DE-005 | **`struct file` UAF** via type confusion on `ep_item_poll` | Thread A holds `ep->mtx` during `__ep_remove`; any `epoll_wait`/`EPOLL_CTL_MOD` blocks on same mutex; by the time Thread D acquires lock, epitem unlinked and RCU-scheduled | VER-025 (EXP-015) — Source + disassembly audit | 2026-07-31 |
| DE-006 | **`struct file` UAF** via `pipe_buffer` cross-cache reclaim | `filp` cache has `SLAB_TYPESAFE_BY_RCU` (unmergeable); `pipe_buffer` sizes (40, 80, 160, 320, 640) mathematically miss 256-byte class | VER-018 (EXP-010) — Source audit of `fs/file_table.c` and `fs/pipe.c` | 2026-07-29 |
| DE-007 | **`struct epitem` UAF** via same-cache reclaim (`EPOLL_CTL_ADD`) | `list_del_init(&epi->rdllink)` corrupts offsets 24/32 BEFORE `call_rcu`; new epitem's `INIT_LIST_HEAD` overwrites corruption | VER-016 (EXP-008) — GDB trace shows `list_del_init` hit before reclaim | 2026-07-28 |
| DE-008 | **`snd_timer_user`** reclaims freed `epitem`/`eventpoll` | `snd_timer_user` (~168B) in `kmalloc-192`; `epitem` (120B) in dedicated `eventpoll_epi` cache; different caches = no reclaim | EVO-005 correction (was VER-006/007) | 2026-07-24 |
| DE-009 | **`snd_timer_user` mutex slowpath** corruption via `wait_list` | Mutex contention was normal, not corruption; `wait_list.next` self-pointing = valid state | VER-013 (EXP-013) — GDB trace shows normal behavior | 2026-07-24 |

---

## Primitive Expansions (Disproven)

| ID | Path | Why Dead | Killing Evidence | Date |
|----|------|----------|------------------|------|
| DE-010 | **Multi-epitem kernel-pointer write** at offset 160 | Requires `f_ep != NULL` → no lockless bypass → inner_epoll not freed → write to LIVE memory | VER-033 (EXP-024) — Hardware watchpoint captures write to live memory | 2026-08-02 |
| DE-011 | **`rb_erase_cached` arbitrary write** via fake rb_node pointers | Operates on OUTER epoll's `rbr` (valid), not freed inner; single-epitem tree writes NULL to offset 104 | VER-032 (EXP-023b) — GDB trace confirms outer epoll valid | 2026-08-01 |
| DE-012 | **`spin_lock_irq`/`spin_unlock_irq` lock corruption** at offset 96 | Operates on OUTER epoll's `lock` (valid), not freed inner | VER-032 (EXP-023b) | 2026-08-01 |
| DE-013 | **KASLR defeat via dual-watch leak** | Depends on DE-002 | VER-030 retracted by VER-033 | 2026-08-02 |

---

## Spray/Reclaim Paths (Disproven or Limited)

| ID | Path | Why Dead | Killing Evidence | Date |
|----|------|----------|------------------|------|
| DE-014 | **`pipe_buffer`** spray for `struct file` reclaim | Cache isolation + size mismatch (see DE-006) | VER-018 (EXP-010) | 2026-07-29 |
| DE-015 | **`snd_timer_user`** spray for `epitem` reclaim | Different cache (see DE-008) | EVO-005 | 2026-07-24 |
| DE-016 | **Dedicated cache merge** (`eventpoll_epi` → `kmalloc-128`) | `SLAB_ACCOUNT` flag prevents merge; `CONFIG_MEMCG=n` means no `kmalloc-cg-128` | VER-014 (STATIC audit of `slab_common.c`) | 2026-07-27 |
| DE-017 | **Cross-cache grooming** generic kmalloc → `eventpoll_epi` | `eventpoll_epi` is truly isolated dedicated cache | VER-014 | 2026-07-27 |

---

## Initial Misconceptions (Retracted Early)

| ID | Path | Why Dead | Killing Evidence | Date |
|----|------|----------|------------------|------|
| DE-018 | **`close(evfd)` vs `epoll_ctl(DEL)` simple race** | True vulnerability requires lockless fast-path bypass via `f_ep=NULL` | FAILURE_ANALYSIS.md §1 | 2026-07-17 |
| DE-019 | **Timing spikes = race proof** | Spikes = lock contention, not UAF; need hardware watchpoint | FAILURE_ANALYSIS.md §2 | 2026-07-17 |
| DE-020 | **Level 3 "Behavior Reproduced" claim** | No kernel-side evidence (crash/KASAN/OOPS) | FAILURE_ANALYSIS.md §3 | 2026-07-17 |
| DE-021 | **`eventpoll_release_file` mutex path** | Lockless fast-path in `eventpoll_release()` bypasses it entirely | VER-024 DISPROVED, replaced by VER-026 | 2026-07-31 |

---

## Revisit Conditions

A dead end MAY be revisited ONLY if:

1. **New kernel configuration** changes the structural constraint (e.g., `CONFIG_PREEMPT=y` adds preemption points)
2. **New source code discovery** reveals missed code path
3. **New hardware capability** changes primitive (e.g., MTE changes crash behavior)

**Revisit requires**: New VER entry with RUNTIME evidence contradicting the killing evidence.

---

## Summary Statistics

| Category | Count |
|----------|-------|
| Exploitation Chains | 4 |
| UAF Target Objects | 5 |
| Primitive Expansions | 4 |
| Spray/Reclaim Paths | 4 |
| Initial Misconceptions | 4 |
| **Total Dead Ends** | **21** |

---

**Last Updated**: 2026-08-02
**Next Review**: After NAT-001/002/AND-001 completion (only if new evidence contradicts)