# Assumptions Register

> **Status**: ACTIVE — All assumptions tracked from repository inception
> **Rule**: Every assumption must have a status and evidence reference
> **Update**: After every experiment that tests or falsifies an assumption

---

## Status Definitions

| Status | Meaning |
|--------|---------|
| **UNTESTED** | Assumption made, no experiment designed yet |
| **TESTED** | Experiment executed, result known |
| **VALIDATED** | Evidence supports assumption |
| **FALSIFIED** | Evidence contradicts assumption |
| **RETIRED** | Assumption no longer relevant (superseded by new understanding) |

---

## Core Vulnerability Assumptions

| ID | Assumption | Status | Evidence | Notes |
|----|------------|--------|----------|-------|
| A-001 | CVE-2026-46242 exists in target kernel (commit `7e35917775b8`) | VALIDATED | VER-026 (EXP-015) | Source code confirms vulnerable path |
| A-002 | Race is between outer-close (Thread A) and inner-close (Thread B) | VALIDATED | VER-026 (EXP-015) | Hardware watchpoint trace |
| A-003 | Lockless fast-path in `eventpoll_release()` enables bypass | VALIDATED | VER-026 (EXP-015) | Thread B never hit `eventpoll_release_file` |
| A-004 | Freed object is `struct eventpoll` in `kmalloc-192` | VALIDATED | VER-020, VER-026 | `sizeof` = 176 → kmalloc-192 |
| A-005 | Only UAF write is `hlist_del_rcu` at offset 160 (NULL) | VALIDATED | VER-028, VER-031, VER-032 | All other ops use OUTER epoll |
| A-006 | Single-epitem vs multi-epitem conditions are mutually exclusive | VALIDATED | VER-033 (EXP-024) | Source logic at line 826 |
| A-007 | `msg_msg` (144B user data) reclaims freed eventpoll | DEBUGGER-ASSISTED | VER-027 (EXP-018/019) | Only with GDB 2-3s window |
| A-008 | No viable exploitation beyond DoS (NULL write only) | PARTIALLY TESTED | VER-028, EXP-016 | Depends on A-007 natural reachability |

---

## Exploitation Chain Assumptions

| ID | Assumption | Status | Evidence | Notes |
|----|------------|--------|----------|-------|
| A-009 | Chain 0: `percpu_counter_dec` dereferences freed ep->user | FALSIFIED | VER-028 (EXP-019) | Uses OUTER epoll's user |
| A-010 | Chain 1: Dual-watch leaks kernel pointer via `msgrcv` | FALSIFIED | VER-033 (EXP-024) | Writes to LIVE memory |
| A-011 | Chain 2: Arbitrary decrement via fake `user_struct` | FALSIFIED | VER-031 (EXP-023b) | `ep` param is OUTER epoll |
| A-012 | Chain 3: Full LPE via modprobe_path/cred corruption | UNTESTED | — | Depends on A-009/011 |
| A-013 | `struct file` UAF via type confusion is reachable | FALSIFIED | VER-025 (EXP-015) | `ep->mtx` barrier |
| A-014 | `epitem` same-cache reclaim yields exploit primitive | FALSIFIED | VER-016 (EXP-008) | `list_del_init` before reclaim |

---

## Allocator & Kernel Config Assumptions

| ID | Assumption | Status | Evidence | Notes |
|----|------------|--------|----------|-------|
| A-015 | `CONFIG_SLAB_FREELIST_RANDOM=n` → deterministic reclaim | VALIDATED | Kernel config | `.config` verified |
| A-016 | `CONFIG_SLAB_FREELIST_HARDENED=n` → no pointer obfuscation | VALIDATED | Kernel config | `.config` verified |
| A-017 | `CONFIG_INIT_ON_FREE_DEFAULT_ON=n` → stale data survives | VALIDATED | Kernel config | `.config` verified |
| A-018 | `CONFIG_INIT_ON_ALLOC_DEFAULT_ON=n` → no zeroing on alloc | VALIDATED | Kernel config | `.config` verified |
| A-019 | `CONFIG_MEMCG=n` → no cgroup cache isolation | VALIDATED | Kernel config | `.config` verified |
| A-020 | `CONFIG_SLUB_CPU_PARTIAL=y` → per-CPU partial slabs exist | VALIDATED | Kernel config | May interfere with reclaim |
| A-021 | `msg_msg` uses generic `kmalloc-192` (not dedicated cache) | VALIDATED | VER-027, source audit | `load_msg` uses `kmalloc` |
| A-022 | `eventpoll` uses generic `kmalloc-192` (not dedicated cache) | VALIDATED | VER-020, source audit | `kzalloc(sizeof(*ep))` |
| A-023 | `eventpoll_epi` (epitem) is dedicated cache, isolated | VALIDATED | VER-014 | `SLAB_ACCOUNT` prevents merge |

---

## Android-Specific Assumptions

| ID | Assumption | Status | Evidence | Notes |
|----|------------|--------|----------|-------|
| A-024 | `msgsnd`/`msgrcv` work in target Android context (shell/app) | UNTESTED | — | **CRITICAL BLOCKER** |
| A-025 | SELinux enforcing allows `epoll`, `msgget`, `msgsnd`, `msgrcv` | UNTESTED | — | **CRITICAL BLOCKER** |
| A-026 | KASLR on device doesn't prevent race/spray | UNTESTED | — | AND-002 planned |
| A-027 | MTE/KASAN_HW_TAGS doesn't prevent UAF exploitation | UNTESTED | — | AND-004 planned |
| A-028 | PAC/BTI irrelevant for data-only NULL write primitive | HYPOTHESIS | — | No control-flow hijack |
| A-029 | `PREEMPT_VOLUNTARY` on device same as QEMU | UNTESTED | — | Need device kernel config |
| A-030 | `CONFIG_USERFAULTFD=n` on device (no uffd timing) | UNTESTED | — | AVD config may differ |

---

## Timing & Scheduler Assumptions

| ID | Assumption | Status | Evidence | Notes |
|----|------------|--------|----------|-------|
| A-031 | Race window exists at `__ep_remove` after `WRITE_ONCE` | DEBUGGER-ASSISTED | EXP-015/018/019 | GDB patch creates artificial point |
| A-032 | Natural preemption point at `cond_resched` in `ep_unregister_pollwait` | HYPOTHESIS | Source audit (line 888) | NAT-002 to test |
| A-033 | 2-CPU QEMU scheduling approximates device | UNTESTED | — | virt vs physical CPU differ |
| A-034 | `SCHED_FIFO` + CPU pinning enables stochastic hit | HYPOTHESIS | — | RUNNER_GUIDE claims 0.01-1% |
| A-035 | No other preemption points in `__ep_remove` | VALIDATED | Source audit | No `cond_resched`/`might_resched` |

---

## Memory Ordering Assumptions (ARM64)

| ID | Assumption | Status | Evidence | Notes |
|----|------------|--------|----------|-------|
| A-036 | `WRITE_ONCE`/`READ_ONCE` sufficient for ARM64 ordering | HYPOTHESIS | — | No explicit barriers in source |
| A-037 | GDB patch doesn't accidentally add memory barriers | VALIDATED | EXP-015 | But patch creates artificial window |
| A-038 | `spin_lock`/`spin_unlock` provide full barrier | VALIDATED | Kernel semantics | But lock released before UAF write |

---

## Experimental Methodology Assumptions

| ID | Assumption | Status | Evidence | Notes |
|----|------------|--------|----------|-------|
| A-039 | GDB-infinite-loop patch models real preemption | FALSIFIED | Adversarial review | Creates artificial preemption point |
| A-040 | 2-3 second GDB spray window ≈ natural timing | FALSIFIED | Adversarial review | Orders of magnitude longer |
| A-041 | Hardware watchpoint doesn't affect race timing | VALIDATED | EXP-015, 024 | Read-only watchpoints |
| A-042 | Single successful run = proof of exploitability | FALSIFIED | Protocol Rule 3 | Requires statistical evidence |

---

## Update Log

| Date | Assumption | Old Status | New Status | Experiment |
|------|------------|------------|------------|------------|
| 2026-08-02 | A-007, A-031, A-039, A-040, A-042 | VALIDATED | DEBUGGER-ASSISTED / FALSIFIED | Adversarial review |
| 2026-08-02 | A-009 | HYPOTHESIS | FALSIFIED | VER-028 (EXP-019) |
| 2026-08-02 | A-010 | HYPOTHESIS | FALSIFIED | VER-033 (EXP-024) |
| 2026-08-02 | A-011 | HYPOTHESIS | FALSIFIED | VER-031 (EXP-023b) |
| 2026-08-02 | A-013 | HYPOTHESIS | FALSIFIED | VER-025 (EXP-015) |
| 2026-08-02 | A-014 | HYPOTHESIS | FALSIFIED | VER-016 (EXP-008) |

---

**Next Review**: After NAT-001, NAT-002, AND-001 completion