# Unknowns Register

> **Status**: ACTIVE — Questions whose answers could materially change conclusions
> **Rule**: Only include questions with binary, testable answers that affect final verdict
> **Format**: Each unknown must have a discriminating experiment

---

## Critical Unknowns (Directly Affect "DoS Only" Conclusion)

| ID | Question | Why It Matters | Discriminating Experiment | Expected Resolution |
|----|----------|----------------|---------------------------|---------------------|
| U-001 | **Can the race be triggered WITHOUT GDB patching?** | If NO: all race evidence is artifact; vulnerability not practically exploitable | **NAT-001**: 10,000 iterations natural race test with 4 widening techniques | **RESOLVED: NO** — 0 hits in 10,000 iterations (95% CI upper bound 0.0384%) |
| U-002 | **Does a natural preemption point exist in the race window?** | If NO: race window too narrow (< 20 cycles) for scheduler | **NAT-002**: Audit `cond_resched` at lines 888/903 + mechanism analysis | **RESOLVED: NO** — cond_resched is no-op in 2-CPU pinned PREEMPT_VOLUNTARY; multi-epitem gives only instruction-count window (~250-550 cycles) |
| U-003 | **Does `msg_msg` reclaim work under natural timing (no GDB 2-3s window)?** | If NO: spray unreliable; exploit fails even if race hits | **NAT-003**: Reclaim stats from NAT-001 runs | YES: ≥10% exact match; NO: only GDB window works |
| U-004 | **Does SysV IPC (`msgsnd`/`msgrcv`) work in target Android context?** | If NO: primary spray blocked; must pivot or fail | **AND-001**: Minimal binary test on AVD shell/app | Binary: syscall succeeds = YES; ENOSYS/EACCES = NO |
| U-005 | **Does SELinux enforcing block required syscalls?** | If YES: exploit blocked on production devices | **AND-003**: Test all primitives in enforcing mode | Per-syscall: allowed/denied |

---

## High-Impact Unknowns (Affect Exploit Reliability)

| ID | Question | Why It Matters | Discriminating Experiment | Expected Resolution |
|----|----------|----------------|---------------------------|---------------------|
| U-006 | **Does KASLR reduce race hit rate to zero?** | If YES: KASLR is de facto mitigation | **AND-002**: NAT-001 with KASLR on vs off | Ratio: hit_rate(KASLR)/hit_rate(nokaslr) |
| U-007 | **Does `SLUB_CPU_PARTIAL` prevent cross-CPU reclaim?** | If YES: Thread A/B must share CPU for spray | **NAT-004**: Same-CPU vs cross-CPU hit rate | Significant difference = YES |
| U-008 | **Does MTE/KASAN_HW_TAGS crash on UAF write?** | If YES: hardware detects UAF before exploitation | **AND-004**: NAT-001 with `kasan=on` + MTE | Crash type: MTE tag fault vs NULL deref |
| U-009 | **Do alternative sprays (`add_key`, `setxattr`) work if msg_msg blocked?** | Fallback if U-004 = NO | **NAT-005**: Spray test + reclaim verification | Binary per primitive |
| U-010 | **Can explicit `sched_yield` at `ep_unregister_pollwait` enable race?** | If YES: timing optimization needed | **NAT-002**: cond_resched is no-op; explicit sched_yield syscall would yield but adds ~1000+ cycles overhead | Unlikely to help — yield overhead exceeds race window |

---

## Medium-Impact Unknowns (Affect PoC Polish)

| ID | Question | Why It Matters | Discriminating Experiment |
|----|----------|----------------|---------------------------|
| U-011 | What is exact `init_user` address for safe `percpu_counter_dec`? | Needed for Chain 1 info leak survival | **VER-031**: `p &init_user` in GDB |
| U-012 | What is `modprobe_path` address for Chain 2? | Needed for arbitrary decrement target | **VER-032**: `p &modprobe_path` in GDB |
| U-013 | Does `fib6_info` spray yield better DoS than `msg_msg`? | EXP-016 identified as crash candidate | Build test: netlink route + race |
| U-014 | Does `snd_timer_user` spray work on Android? | EXP-016 candidate; may be blocked by SELinux | **AND-003**: open `/dev/snd/timer` test |
| U-015 | Does `packet_fanout` spray work on Android? | EXP-016 candidate; needs AF_PACKET + namespace | Build test: fanout socket + race |

---

## Resolved Unknowns (Moved to Assumptions Register)

| ID | Question | Resolution | Evidence |
|----|----------|------------|----------|
| U-016 | Is `struct eventpoll` in kmalloc-192? | YES (176 bytes → kmalloc-192) | VER-020 |
| U-017 | Is `hlist_del_rcu` the ONLY UAF write? | YES (offset 160, NULL) | VER-028, 031, 032 |
| U-018 | Are single-epitem and multi-epitem mutually exclusive? | YES (line 826 logic) | VER-033 (EXP-024) |
| U-019 | Does Chain 0 (`percpu_counter_dec`) work? | NO (uses OUTER epoll) | VER-028 (EXP-019) |
| U-020 | Does Chain 1 (dual-watch leak) work? | NO (writes to LIVE memory) | VER-033 (EXP-024) |
| U-021 | Does Chain 2 (arbitrary decrement) work? | NO (`ep` param is OUTER) | VER-031 (EXP-023b) |
| U-022 | Does `struct file` UAF yield type confusion? | NO (`ep->mtx` barrier) | VER-025 (EXP-015) |
| U-023 | Does `epitem` same-cache reclaim work? | NO (`list_del_init` before reclaim) | VER-016 (EXP-008) |
| U-024 | Does `snd_timer_user` reclaim eventpoll? | NO (different caches) | EVO-005 correction |

---

## Tracking Format

For each unknown, track:

```
U-XXX: [Question]
Status: UNTESTED | TESTING | RESOLVED
Experiment: [Experiment ID]
Target Resolution: [Date]
Result: [Once resolved]
```

---

**Next Review**: After NAT-001, NAT-002, AND-001 completion