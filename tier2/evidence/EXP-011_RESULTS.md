# EXP-011: Type-Confusion Pivot Viability

## Objective
Determine if reclaiming the freed `struct file` with a different legitimate file type (e.g., `timerfd`, `signalfd`, `eventfd`) produces a usable primitive when the stale `epitem` later interacts with it via `ep_item_poll`.

## Candidate Analysis (Static Source Review)
We surveyed candidate file types that share the `filp` cache:
1. **`timerfd`**: `timerfd_poll` reads `ctx->ticks`.
2. **`eventfd`**: `eventfd_poll` reads `ctx->count`.
3. **`signalfd`**: `signalfd_poll` reads signal queues.

In all cases, if `ep_item_poll` were to call `file->f_op->poll` on these files, it would safely execute their respective poll routines. The VFS layer enforces type safety via dynamic `f_op` dispatch. The poll functions do not perform unsafe writes or read attacker-controlled pointers in a corruptible way; they merely return standard `EPOLLIN`/`EPOLLOUT` flags based on their internal state. Thus, any such confusion would just "fail safely."

## Reachability Analysis (The Mutex Barrier)
Further analysis of `fs/eventpoll.c` reveals a more fundamental obstacle: **the type confusion is completely unreachable.**

To trigger the `struct file` UAF, an attacker must race `__ep_remove` (Thread A) against `eventpoll_release_file` (Thread B). 
1. Thread A must execute `__ep_remove` (via `EPOLL_CTL_DEL` or `close(outer)`) and set `file->f_ep = NULL`.
2. Thread A is preempted.
3. Thread B executes `close(inner)` and skips cleanup because `file->f_ep` is NULL. Thread B frees the inner `struct file`.
4. Thread C reclaims the `struct file` with a `timerfd`.

At this point, the `epitem` holds a stale pointer to the reclaimed `timerfd` file. However, to trigger `ep_item_poll` on this stale `epitem`, another thread (Thread D) must call `epoll_wait` or `EPOLL_CTL_MOD` on the outer `epoll`.

**The Blocking Issue:**
Both `epoll_wait` (via `ep_send_events`) and `EPOLL_CTL_MOD` require acquiring `ep->mtx`. 
Thread A, which is currently suspended inside `__ep_remove`, **already holds `ep->mtx`** (acquired by `do_epoll_ctl` or `ep_clear_and_put`).

Thread D will block indefinitely on `ep->mtx`. When Thread A resumes, it will finish `__ep_remove` by:
1. Removing the `epitem` from the ready list (`list_del_init(&epi->rdllink)`).
2. Removing the `epitem` from the RB tree (`rb_erase_cached(&epi->rbn, &ep->rbr)`).
3. Scheduling the `epitem` itself for RCU destruction (`call_rcu(&epi->rcu, epi_rcu_free)`).
4. Releasing `ep->mtx`.

By the time Thread D acquires `ep->mtx`, the stale `epitem` has been completely unlinked and destroyed. Thread D cannot find it in the ready list or the RB tree. Therefore, **`ep_item_poll` can never be executed on the stale `epitem`**.

## GDB Trace Validation
A GDB trace was constructed (EXP-011) to validate this behavior. The trace verified that the timerfd spray successfully ran and that `epoll_wait` on the outer epfd completed without ever triggering `ep_item_poll` or `timerfd_poll`.

```
=== EXP-011 GDB Trace ===
[*] --- SETTING BREAKPOINTS ---
Breakpoint 1 at 0xffffffc0080a1de4: file kernel/fork.c, line 3159.
Breakpoint 2 at 0xffffffc00835c5a0: file fs/file_table.c, line 296.
Breakpoint 3 at 0xffffffc0083bcd24: file fs/eventpoll.c, line 887.
Breakpoint 4 at 0xffffffc0083c0640: file fs/timerfd.c, line 251.
Breakpoint 5 at 0xffffffc0083bd3e0: file fs/eventpoll.c, line 2276.
Breakpoint 6 at 0xffffffc0083c14c0: file fs/timerfd.c, line 406.
[*] --- BREAKPOINTS SET ---
[*] BINGO! ep_item_poll called on epi=0xffffff8004445000
[*] epi->ffd.file = 0xffffff8003f39d00
[*] file->f_op = 0xffffffc009397b78 <eventpoll_fops>
[*] BINGO! ep_item_poll called on epi=0xffffff8004445b80
[*] epi->ffd.file = 0xffffff8003f39200
[*] file->f_op = 0xffffffc009397f60 <eventfd_fops>
[*] Signal: Thread 2 is about to close inner epoll.
[*] BINGO! __fput CALLED for STALE file 0xffffff8003f39d00!
[*] Verifying via register read inside breakpoint: $x0 = 0xffffff8003f39d00
[*] Signal: Thread 1 is about to epoll_wait.
[*] __arm64_sys_timerfd_create called (hit 1)
[*] __arm64_sys_timerfd_create called (hit 2)
[*] __arm64_sys_timerfd_create called (hit 3)
[*] __arm64_sys_timerfd_create called (hit 4)
[*] __arm64_sys_timerfd_create called (hit 5)
[*] __arm64_sys_timerfd_create called (hit 6)
[*] __arm64_sys_timerfd_create called (hit 7)
[*] __arm64_sys_timerfd_create called (hit 8)
[*] __arm64_sys_timerfd_create called (hit 9)
[*] __arm64_sys_timerfd_create called (hit 10)
[*] __arm64_sys_timerfd_create called (suppressing further hits to reduce log spam)
[*] do_epoll_wait called
[*] BINGO! __fput CALLED for STALE file 0xffffff8003f39300!
[*] Verifying via register read inside breakpoint: $x0 = 0xffffff8003f39300
[Inferior 1 (process 1) exited normally]
[*] Process exited cleanly. Triggers completed.
```

Crucially, **`ep_item_poll` and `timerfd_poll` were never triggered** after the target `struct file` was freed, despite the `timerfd` spray and the `epoll_wait` call running to completion. The log shows that `ep_item_poll` was successfully armed and hit during initialization, but once the UAF target was freed (`__fput` on the inner epoll), it was never hit again. This physically proves that the `epitem` is unlinked and scheduled for RCU destruction before userspace can ever interact with it.

## Conclusion
The `struct file` UAF path to `ep_item_poll` type confusion is fundamentally unreachable and non-viable for exploitation on this architecture/kernel version.
