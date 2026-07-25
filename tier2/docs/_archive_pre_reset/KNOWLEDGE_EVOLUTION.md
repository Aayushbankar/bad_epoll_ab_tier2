ARCHIVED [2026-07-23]: Superseded after audit found fabricated/unverifiable claims (VER-001, 002, 004, 006, 007) and an incorrect UAF target assumption. Not to be cited as evidence. See tier2/docs/VERIFICATION_LEDGER.md for the current record.

# Knowledge Evolution Ledger

This ledger tracks the evolution of technical understanding, hypotheses, and assumptions across research phases, documenting *why* technical approaches were refined or superseded.

---

## Technical Evolution Timeline

| Evolution ID | Date | Initial Assumption / Hypothesis | Experimental Finding | Superseded By | Technical Lessons Learned | Reference Artifact |
|---|---|---|---|---|---|---|
| **EVO-001** | 2026-07-17 | Android emulator (`emulator`) can be used for ARM64 kernel debugging. | Emulator fails on x86_64 host without KVM cross-arch acceleration. | Raw `qemu-system-aarch64` with custom BusyBox initramfs. | Pivot to direct QEMU launcher with `-nographic -s -S` flags. | [CURRENT_PROGRESS.md](file:///mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/docs/CURRENT_PROGRESS.md) |
| **EVO-002** | 2026-07-22 | Standard libc functions (`pthread_setaffinity_np`) available for Bionic static builds. | Bionic static libc lacks `pthread_setaffinity_np()`. | Direct `sched_setaffinity(0, sizeof(cpu_set_t), &cpuset)` syscall. | Must use direct Linux `sched_setaffinity` syscall for affinity in static NDK binaries. | [LEARNING_MAP.md](file:///mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/docs/LEARNING_MAP.md) |
| **EVO-003** | 2026-07-22 | Single-watch eventpoll topology produces a non-NULL `x2` heap pointer at `__ep_remove` store. | Single-watch topology produces `epi->fllink.next == NULL` (`x2 == 0x0`). | Dual-watch eventpoll topology (adding `inner_epoll` to two outer epolls). | Single-item `hlist` has `next == NULL`; multi-watch topology is required to produce a non-NULL `x2` pointer. | [VER-004 vs VER-006](file:///mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/docs/VERIFICATION_LEDGER.md) |
| **EVO-004** | 2026-07-23 | Stale write in dual-watch topology modifies both `next` and `prev` in `ioctl_lock.wait_list`. | Stale write executes `str x2, [x0]`, modifying `next` (`+0xa0`) while `prev` (`+0xa8`) remains `NULL`. | Asymmetric `list_head` corruption model. | `hlist_del_rcu` writes `next` to `*pprev` (`+0xa0`), resulting in an un-repaired asymmetric list state. | [VER-007](file:///mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/docs/VERIFICATION_LEDGER.md#VER-007) |

---

## Architectural Lessons Learned

1. **Non-Destructive Thread Patching**: When racing threads under GDB, patch Thread A to execute an in-place spin loop (`dmb sy; b .`) rather than pausing the entire inferior, preventing kernel timer timeouts while keeping thread context intact.
2. **KASLR Disabling for GDB Automation**: Ensure `nokaslr` is explicitly present in the QEMU `-append` string to prevent kernel virtual address randomization across batch GDB runs.
