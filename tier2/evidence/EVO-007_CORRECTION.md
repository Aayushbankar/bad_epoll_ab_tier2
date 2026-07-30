# EVO-007: Correction of the UAF Victim (Restoring Tier 1 Theory)

## Executive Summary
EVO-005 and VER-011 incorrectly identified the UAF victim as `struct epitem` (120 bytes) in the `eventpoll_epi` slab cache and the stale write as `list_del_init(&epi->rdllink)`. **This is fundamentally incorrect.**

The original Tier 1 x86 exploit theory holds true for the Android aarch64 Tier 2 port:
1. The UAF victim is **`struct eventpoll`**, which is allocated via `kmalloc-192`.
2. The stale write is **`hlist_del_rcu(&epi->fllink)`**, which writes `NULL` (0) to offset `160` (0xa0) of the freed `struct eventpoll`.

## Detailed Root Cause Correction
The CVE-2026-46242 race condition triggers when Thread A closes `outer_epoll` and Thread B closes `inner_epoll` (target file).

1. Thread A (`close(outer_epoll)`) calls `ep_clear_and_put`, which calls `__ep_remove(epi)`.
2. Thread A's `__ep_remove` clears `inner_epoll->f_ep` by executing `WRITE_ONCE(file->f_ep, NULL);` under `file->f_lock`.
3. Thread B (`close(inner_epoll)`) calls `__fput`, which calls `eventpoll_release(inner_epoll)`.
4. Thread B's `eventpoll_release` sees `f_ep == NULL` locklessly and returns, skipping `eventpoll_release_file`.
5. Thread B continues and calls `inner_epoll->f_op->release`, which frees `inner_epoll`'s `struct eventpoll` via `kfree(ep)`. `struct eventpoll` is 176 bytes and lives in `kmalloc-192`.
6. Thread A's `__ep_remove` continues and executes `hlist_del_rcu(&epi->fllink)`.
7. `epi->fllink.pprev` points to `&inner_epoll->refs.first` (offset 160 / 0xa0 of `struct eventpoll`).
8. `hlist_del_rcu` writes `epi->fllink.next` (which is NULL) to `*pprev`.
9. **Result**: A UAF write of `NULL` to offset 160 of the freed `kmalloc-192` chunk.

## Why EVO-005 Was Wrong
EVO-005 incorrectly assumed that because `struct epitem` is freed by `kfree_rcu(epi, rcu)` at the end of `__ep_remove`, it was the UAF victim. Furthermore, it assumed that `list_del_init(&epi->rdllink)` was a stale write. This is false because `list_del_init` executes *before* `epi` is ever passed to `kfree_rcu`. The memory is valid when `list_del_init` executes. The *actual* UAF write targets `target_file->private_data` (`struct eventpoll`), which is freed concurrently by the other thread.

## GDB Verification (Static)
We verified the exact size and offset dynamically in GDB:
```gdb
pwndbg> printf "eventpoll size: %d\n", sizeof(struct eventpoll)
eventpoll size: 176
pwndbg> print &(((struct eventpoll *)0)->refs)
$1 = (struct hlist_head *) 0xa0
```
- Size: 176 bytes (`kmalloc-192`).
- Offset of `refs`: 0xa0 (160 bytes).

## Impact on Exploit Strategy
Because the UAF victim is `kmalloc-192`, **cross-cache reclaim is fundamentally viable**. We are no longer restricted to same-cache `epoll_ctl` reclaim in the isolated `eventpoll_epi` cache. We can spray ANY `kmalloc-192` object over the freed `struct eventpoll` and corrupt 8 bytes at offset 160 with NULL.
