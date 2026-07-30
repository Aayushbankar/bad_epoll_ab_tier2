# EVO-007: Theory of the UAF Victim (Restoring Tier 1 Theory)

## Executive Summary
This document proposes a return to the original Tier 1 x86 exploit theory for the Android aarch64 Tier 2 port. Note that this is currently **THEORY ONLY** and lacks live-trace verification.

1. The UAF victim is theorized to be **`struct eventpoll`**, which is allocated via `kmalloc-192`.
2. The stale write is theorized to be **`hlist_del_rcu(&epi->fllink)`**, which writes `NULL` (0) to offset `160` (0xa0) of the freed `struct eventpoll`.

> [!WARNING]
> This is the exact same struct/cache/offset/write theory that was claimed in VER-006 and VER-007, which were retracted as fabricated prior to the full reset. There is currently **NO NEW LIVE-TRACE EVIDENCE** justifying the revival of this theory; it is an independent re-derivation based purely on static struct-layout facts and source code analysis. It remains an unverified assumption until proven by a live GDB trace matching the standard of EXP-009.

## Detailed Root Cause Theory
The CVE-2026-46242 race condition triggers when Thread A closes `outer_epoll` and Thread B closes `inner_epoll` (target file).

1. Thread A (`close(outer_epoll)`) calls `ep_clear_and_put`, which calls `__ep_remove(epi)`.
2. Thread A's `__ep_remove` clears `inner_epoll->f_ep` by executing `WRITE_ONCE(file->f_ep, NULL);` under `file->f_lock`. This fast-path only triggers if `epi` is the *sole* entry on `file->f_ep` (`head->first == &epi->fllink && !epi->fllink.next`).
3. Thread B (`close(inner_epoll)`) calls `__fput`, which calls `eventpoll_release(inner_epoll)`.
4. Thread B's `eventpoll_release` sees `f_ep == NULL` locklessly and returns, skipping `eventpoll_release_file`.
5. Thread B continues and calls `inner_epoll->f_op->release`, which frees `inner_epoll`'s `struct eventpoll` via `kfree(ep)`. `struct eventpoll` is 176 bytes and lives in `kmalloc-192`.
6. Thread A's `__ep_remove` continues and executes `hlist_del_rcu(&epi->fllink)`.
7. `epi->fllink.pprev` points to `&inner_epoll->refs.first` (offset 160 / 0xa0 of `struct eventpoll`).
8. `hlist_del_rcu` writes `epi->fllink.next` (which is NULL) to `*pprev`.
9. **Result**: A UAF write of `NULL` to offset 160 of the freed `kmalloc-192` chunk.

## Reconciliation with EXP-009
EXP-009 successfully demonstrated a register-verified UAF on `struct file` (specifically, `epi->ffd.file` holding a reference to a freed `struct file`). That result remains verified.

Are both UAFs real and coexisting? **Yes, theoretically.**
In the same race condition where Thread A and Thread B race to close their respective epoll fds:
- Thread A retains a pointer to the freed `inner_epoll`'s `struct file` in `epi->ffd.file`.
- Thread A also retains a pointer to the freed `inner_epoll`'s `struct eventpoll` (via `epi->fllink.pprev`).

Both objects are freed by Thread B (`kfree(ep)` and `file_free(file)`). Thread A later operates on both dangling pointers. The `struct file` UAF is proven (EXP-009). The `struct eventpoll` UAF is currently just a theory.

## GDB Verification (Static Only)
We verified the exact size and offset statically in GDB:
```gdb
pwndbg> printf "eventpoll size: %d\n", sizeof(struct eventpoll)
eventpoll size: 176
pwndbg> print &(((struct eventpoll *)0)->refs)
$1 = (struct hlist_head *) 0xa0
```
- Size: 176 bytes (`kmalloc-192`).
- Offset of `refs`: 0xa0 (160 bytes).

This does not prove the `hlist_del_rcu` write occurs *after* the `kfree`. A live trace is required.

## Live Trace Requirements
To promote this theory to a verified fact, a live trace (e.g., EXP-013) must be executed that captures:
1. Thread B's `kfree` of the `inner_epoll`'s `struct eventpoll` (with timestamp/breakpoint hit).
2. Thread A's `hlist_del_rcu` write (with timestamp/breakpoint hit) occurring *after* Thread B's free.
3. A memory read of the target address post-write confirming the `NULL` landed in genuinely freed memory (not still-valid memory that looks similar).
4. Confirmation that the topology precondition (`epi` is the sole entry on `file->f_ep`) was satisfied.
