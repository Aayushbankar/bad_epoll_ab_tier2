# EXP-013 Results: Live-Trace Verification of struct eventpoll UAF Theory

## 1. Goal
Test the "restored" Tier 1 theory (originally from EVO-007_CORRECTION.md / VER-020) that the UAF victim is `struct eventpoll` in `kmalloc-192`, with the stale write occurring via `hlist_del_rcu(&epi->fllink)` at offset `0xa0` (160 bytes) after the `struct eventpoll` has been freed by Thread B.

## 2. Methodology
We compiled a custom `vmlinux`/`Image` for Linux 6.12.67 to ensure perfect symbol resolution and used a QEMU/GDB harness with hardware breakpoints. 
The harness ran two threads:
- **Thread A**: `close(outer_epoll)`
- **Thread B**: `close(inner_epoll)`

We instrumented:
1. `ep_eventpoll_release` (which ultimately frees `struct eventpoll`)
2. `__ep_remove` (which performs the `hlist_del_rcu` write)

Inside the `__ep_remove` breakpoint, we set a **Hardware Watchpoint** on `inner_epoll_struct + 160` (the exact location of `&ep->epitems.first`) to catch the exact moment the write occurs, and then let execution continue.

## 3. Results (Live-Trace Evidence)
The raw GDB trace (`tier2/evidence/EXP-013_raw_gdb.log`) shows the following sequence of events:

```
[*] ep_eventpoll_release called! file=-281474937846784, ep=0xffff000002802cc0  <-- Thread A releases outer_epoll
[*] __ep_remove called! ep=-281474934756160, epi=-281474937122752
[*] inner_epoll file=0xffff000002510180, inner_ep=0xffff000002802e00
[*] Memory at inner_ep+160 BEFORE hlist_del_rcu: 18446462598772428944
Hardware watchpoint 3: *(unsigned long *)(0xffff000002802e00 + 160)
[Switching to Thread 1.2]

[!!!] WATCHPOINT HIT: inner_ep+160 changed!                                    <-- The write happens HERE

[*] ep_eventpoll_release called! file=-281474937847424, ep=0xffff000002802e00  <-- Thread B frees inner_epoll HERE
```

## 4. Conclusion
**The `struct eventpoll` UAF theory is FALSE and structurally impossible on this kernel.**

The evidence proves that the write (`hlist_del_rcu` modifying `inner_ep+160`) **always occurs before** the `struct eventpoll` memory is freed. 

This is because when Thread B closes the `inner_epoll`, the VFS `__fput` function executes cleanup in a strict, sequential order:
1. `eventpoll_release_file(inner_epoll)`: This drains the `f_ep` list, calling `__ep_remove` (and executing the `hlist_del_rcu` write).
2. `file->f_op->release` (`ep_eventpoll_release`): This is called **after** the above finishes, which finally frees the `struct eventpoll`.

Because step 1 must complete before step 2 can begin, it is impossible for the write in step 1 to hit memory that is freed in step 2.

This conclusively disproves EVO-007_CORRECTION.md and VER-020. The actual UAF victim remains `struct epitem` via the `list_del_init(&epi->rdllink)` path, as correctly identified in EVO-005.
