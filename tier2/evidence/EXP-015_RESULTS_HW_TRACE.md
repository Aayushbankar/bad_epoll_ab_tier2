# EXP-015: Hardware Watchpoint Trace of CVE-202X Lockless Path Race

## Objective
To definitively prove or disprove the EVO-008 theory: that a concurrent multi-threaded execution (Thread A closing outer epoll, Thread B closing inner epoll) can trigger a UAF on the `struct eventpoll` in `kmalloc-192` by exploiting the lockless fast-path check in `eventpoll_release()`.

## Background & Contradiction Resolution
Previous static analysis (VER-024) and single-thread live tracing (EXP-013, VER-021) concluded that `ep_free` (which frees `struct eventpoll`) is strictly synchronized with `__ep_remove` via `eventpoll_release_file()`. This led to the conclusion that the `struct eventpoll` UAF was structurally impossible.

However, this overlooked the exact CVE mechanism: the **lockless fast path** in `include/linux/eventpoll.h:eventpoll_release()`. 

When `__fput` executes, it calls `eventpoll_release(file)` *before* `eventpoll_release_file(file)`. The inline function `eventpoll_release()` performs a lockless read:
```c
static inline void eventpoll_release(struct file *file) {
    if (likely(!READ_ONCE(file->f_ep)))
        return;
    eventpoll_release_file(file);
}
```

If Thread A (in `__ep_remove`) executes `WRITE_ONCE(file->f_ep, NULL)` and is then preempted, Thread B (in `__fput`) will see `f_ep == NULL` and completely bypass `eventpoll_release_file(file)`. Thread B will then proceed to free the `struct eventpoll`, leaving Thread A to wake up and write to the freed memory.

## Methodology
To prove this, `tier2/scripts/exp015_gdb.py` was constructed to force the race condition:
1. **Thread A** executes `close(outer_epoll_fd)`.
2. GDB intercepts Thread A inside `__ep_remove` at offset `+0x19c` (exactly after `WRITE_ONCE(file->f_ep, NULL)` and `spin_unlock(&file->f_lock)`).
3. GDB patches Thread A's PC with an infinite loop instruction (`b .`), artificially suspending it.
4. **Thread B** executes `close(inner_epoll_fd)`.
5. Thread B enters `__fput`, hits the lockless fast path, bypasses `eventpoll_release_file`, and calls `ep_free`.
6. After Thread B finishes `ep_free`, GDB sets a hardware watchpoint on `inner_epoll + 160` (the location of `refs.first`).
7. GDB restores Thread A's original instruction and lets it resume.
8. We observe whether Thread A triggers the hardware watchpoint by writing to the freed memory.

## Results
The experiment successfully triggered the hardware watchpoint exactly as theorized.

### Script Trace Output (`EXP-015_script_trace.log`)
```
Breakpoints set. Continuing execution.
[*] Thread A hit 0xffff8000802bc9c0 (WRITE_ONCE finished).
[*] target_file=-0xfffffdabedc0, target_epoll=-0xfffffcaa7ac0
[*] Original instruction at PC: -0x6effa000
[*] Patched PC with infinite loop to suspend Thread A!
[*] Thread hit __fput(-0xfffffdabedc0)!
[*] Thread B hit ep_free(-0xfffffcaa7ac0)!
[*] Thread B finished ep_free. target_epoll is now FREED!
[*] Set HW watchpoint on -0xfffffcaa7a20
[*] Restored Thread A's instruction at 0xffff8000802bc9c0!
```
*Note: Thread B never hit `eventpoll_release_file`.*

### GDB Hardware Watchpoint Log (`EXP-015_gdb_hw_trace.log`)
```
Hardware watchpoint 6: *(long*)(-281474920774176)

Thread 1 hit Hardware watchpoint 6: *(long*)(-281474920774176)

Old value = -281474920794800
New value = 0
__hlist_del (n=0xffff000003553550) at ./include/linux/list.h:989
989		if (next)
```

## Conclusion
1. **The Race Condition Exists:** The lockless fast path in `eventpoll_release` allows Thread B to bypass cleanup if preempted by Thread A.
2. **The UAF is Real:** Thread A resumes and calls `hlist_del_rcu(&epi->fllink)`, which writes `NULL` (0) to `inner_epoll->refs.first` (offset 160).
3. **The Target Cache is kmalloc-192:** The freed `struct eventpoll` resides in `kmalloc-192`, meaning a UAF write of `0` occurs at offset 160 of a `kmalloc-192` slab object.

This definitively proves EVO-008 and invalidates the single-thread conclusions of VER-021. The root cause is a precise, multi-threaded timing vulnerability.
