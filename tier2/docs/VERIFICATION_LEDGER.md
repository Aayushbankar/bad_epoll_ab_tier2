# Verification Ledger

This document serves as the authoritative, machine-parseable Single Source of Truth (SSOT) mapping every verified research claim to its physical evidence, target kernel symbol, execution method, and raw log artifact.

---

## Verified Claims & Physical Evidence Matrix

| Verification ID | Date & Timestamp (UTC) | Claim / Fact Description | Target Symbol / Address | Verification Method | Raw Evidence File | Status |
|---|---|---|---|---|---|---|
| **VER-001** | 2026-07-22T16:45:00Z | `/dev/snd_timer` character device reachability | `snd_timer_user_open` (`ffffffc008cd7db4`) | Runtime Open Harness (`fd=3`) | [task-180.log](file:///home/legion/.gemini/antigravity-cli/brain/6384332c-74a6-4170-b751-7e40ded0c497/.system_generated/tasks/task-180.log) | VERIFIED |
| **VER-002** | 2026-07-22T17:05:00Z | `snd_timer_user` exact heap chunk reclaim | `inner_epoll` == `snd_timer_user` (`0xffffff8003beb480`) | GDB Batch Memory Trace | [task-245.log](file:///home/legion/.gemini/antigravity-cli/brain/6384332c-74a6-4170-b751-7e40ded0c497/.system_generated/tasks/task-245.log) | VERIFIED |
| **VER-003** | 2026-07-22T17:20:00Z | `snd_timer_user` offset mapping (`ioctl_lock.wait_list`) | `snd_timer_user + 0xa0` (`ioctl_lock.wait_list.next`) | Pahole / GDB Struct Lookup | [vmlinux](file:///mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/android/artifacts/vmlinux) | VERIFIED |
| **VER-004** | 2026-07-22T17:41:00Z | Single-watch topology stale store (`x2 == 0`) | `0xffffffc0083bcedc`: `str x2, [x0]` (`x2 = 0x0`) | GDB Stepi & Memory Dump | [task-329.log](file:///home/legion/.gemini/antigravity-cli/brain/6384332c-74a6-4170-b751-7e40ded0c497/.system_generated/tasks/task-329.log) | VERIFIED |
| **VER-005** | 2026-07-22T23:25:00Z | `attach_epitem()` multi-node `f_ep` list prepending | `hlist_add_head_rcu` in `attach_epitem()` | Source Analysis (`fs/eventpoll.c:1476`) | [eventpoll.c:1476](file:///mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/android/source/common/fs/eventpoll.c#L1476) | VERIFIED |
| **VER-006** | 2026-07-23T00:03:00Z | Dual-watch topology non-NULL `x2` pointer proof | `x2` = `0xffffff800419d250` (`&epi1->fllink`) | Dual-Watch GDB Runtime Trace | [task-366.log](file:///home/legion/.gemini/antigravity-cli/brain/6384332c-74a6-4170-b751-7e40ded0c497/.system_generated/tasks/task-366.log) | VERIFIED |
| **VER-007** | 2026-07-23T00:09:00Z | `ioctl_lock.wait_list` asymmetric corruption proof | `wait_list.next` = `0xffffff800419d250`, `prev` = `0x0` | Pre/Post Store Stepi Dump | [task-396.log](file:///home/legion/.gemini/antigravity-cli/brain/6384332c-74a6-4170-b751-7e40ded0c497/.system_generated/tasks/task-396.log) | VERIFIED |
| **VER-008** | 2026-07-23T00:19:00Z | Mutex slowpath `__list_add` un-repaired dereference | `WRITE_ONCE(prev->next, new)` (`prev == 0x0`) | Source Analysis (`mutex.c:213`, `list.h:75`) | [list.h:75](file:///mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/android/source/common/include/linux/list.h#L75) | VERIFIED |

---

## Verification Maintenance Protocol

1. **Immutability**: Once an entry is assigned a `Verification ID`, its description and evidence link must not be deleted or modified.
2. **Provenance**: Every runtime claim MUST link to an un-truncated log artifact in `tier2/evidence/` or task system logs.
3. **Cross-Referencing**: Articles in `tier2/docs/` citing physical results must link directly to the corresponding `Verification ID`.
