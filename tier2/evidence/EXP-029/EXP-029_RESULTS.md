# EXP-029: Corrected Unassisted E2E PoC (Step 2)

## Executive Summary
- **Objective**: Execute a fully unassisted (no GDB) exploit run with the alignment bug fixed (runtime slab stride) and the safe fallback layout applied. Stop and report the raw results before escalating to a real target.
- **Outcome**: **Negative (No Root)**. The exploit ran to the maximum of 150,000 iterations without successfully reclaiming the victim `struct file`.
- **Key Breakthrough**: The harness achieved **30 natural race wins** and successfully bypassed the intermediate cleanup panics that plagued HYP-013 by leaking the `ep_uaf_waiter` file descriptors on miss.
- **Terminal Panic**: When the harness completed 150,000 iterations and called `exit()`, the kernel forcefully closed the 30 leaked `ep_uaf_waiter` eventpolls. This triggered `ep_eventpoll_release -> ep_clear_and_put -> remove_wait_queue`, resulting in a `list_del` corruption panic because the wait queue still referenced the unreclaimed, freed victim `struct file`s.

## Raw Telemetry (RUNTIME)
As printed by the harness before the terminal panic:
```text
==========================================================
[-] Failed to achieve root within 150000 iterations.
[*] FINAL TELEMETRY:
[*]   Race Wins:       30
[*]   Reclaim Hits:    0
[*]   Fires:           0
[*]   Panics Avoided:  30
[*]   Leaked EPs:      30
[*]   Final uid=2000 euid=2000 (started as uid=2000)
==========================================================
```

## Privilege Drop Verification (RUNTIME)
To explicitly guard against the VER-064 false-success pattern, the harness successfully verified its unprivileged status both before the race loop and in its final telemetry:
```text
[*] PRE-DROP PRIVILEGE CHECK: uid=0 euid=0
[*] POST-DROP PRIVILEGE CHECK: uid=2000 euid=2000
==========================================================
[+] PRIVILEGE DROP CONFIRMED: uid=2000, euid=2000.
[+] Any future getuid()==0 is a GENUINE escalation result.
==========================================================
```

## Slab Geometry and Alignment Fix (RUNTIME)
The alignment bug was successfully resolved by computing the slab stride at runtime:
```text
[*] Slab geometry: object_size=232, slab_size=256, objs_per_slab=16
[*] Using slab stride = 256 bytes (16 objects per page)
```
While `sizeof(struct file)` is 232 bytes, the `SLAB_HWCACHE_ALIGN` flag forces a 64-byte alignment, bringing the actual stride to 256 bytes. The harness correctly spaced the fake objects using this stride.

## The Terminal Panic (RUNTIME)
After printing the final telemetry, the harness process exited naturally. The kernel's file cleanup routine tore down the leaked eventpolls:
```text
[ 3211.505794][   T56] list_del corruption. prev->next should be ffffff80045ea468, but was ffffff8009da91c0. (prev=ffffff8009da91c0)
[ 3211.509321][   T56] ------------[ cut here ]------------
[ 3211.511018][   T56] kernel BUG at lib/list_debug.c:61!
[ 3211.512953][   T56] Internal error: Oops - BUG: 00000000f2000800 [#1] PREEMPT SMP
...
[ 3211.532521][   T56] Call trace:
[ 3211.532944][   T56]  __list_del_entry_valid+0xc8/0xe0
[ 3211.533472][   T56]  remove_wait_queue+0x34/0x80
[ 3211.535322][   T56]  ep_clear_and_put+0x98/0x1dc
[ 3211.536482][   T56]  ep_eventpoll_release+0x20/0x38
[ 3211.537138][   T56]  __fput+0xfc/0x2dc
[ 3211.537895][   T56]  ____fput+0x18/0x2c
[ 3211.538753][   T56]  task_work_run+0xd8/0x104
[ 3211.539821][   T56]  do_exit+0x29c/0xa50
```
This demonstrates that while our `f_ep = empty_zero_page` fallback (EVO-036) prevents a panic during `__ep_remove`, it does not save us from `remove_wait_queue` if the `ep_uaf_waiter` file is closed while the wait queue still references the freed object. Leaking the file descriptor defers this panic until process death.

## Conclusion
The alignment fix and privilege-drop mechanics are fully functional, and the exploit accurately gates itself against firing blindly. However, the Stage 2 reclaim (AAR Oracle Miss) failed 30 times out of 30, meaning the dma-buf spray is still struggling to overlay the freed `struct file`.
