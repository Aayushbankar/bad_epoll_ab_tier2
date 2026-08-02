# Phase 3 Plan: Natural Schedulability & Android Portability

> **Status**: ACTIVE — Supersedes all prior experiment plans
> **Objective**: Produce statistically valid evidence that the CVE-2026-46242 epoll UAF is naturally schedulable on an unmodified Android GKI kernel, or prove it is not.
> **Date**: 2026-08-02
> **Author**: Repository auditor (adversarial review)

---

## 1. Executive Summary

### 1.1 Current State of Evidence

| Category | Status | Key Finding |
|----------|--------|-------------|
| **Race Existence** | **DEBUGGER-ASSISTED ONLY** | VER-026 (EXP-015) proves race with GDB patching PC at `__ep_remove+0x19c` to infinite loop (`b .`). No natural preemption point demonstrated. |
| **Primitive** | **NULL WRITE AT OFFSET 160** | Only confirmed UAF write is `hlist_del_rcu(&epi->fllink)` writing 8-byte NULL to `struct eventpoll.refs.first` (offset 160) in `kmalloc-192`. |
| **Reclaim** | **DEBUGGER-ASSISTED ONLY** | EXP-018/019: `msg_msg` reclaims freed slot with 2-3 second GDB-controlled window. No evidence reclaim works under natural scheduling. |
| **Exploitation Chains** | **ALL DISPROVEN** | Chain 0 (percpu_counter_dec), Chain 2 (arbitrary decrement), Dual-watch KASLR leak — all invalidated by VER-028/031/032/033. |
| **Android Config** | **UNTESTED** | All experiments use `kasan=off nokaslr` + BusyBox init. Production mitigations (SELinux, seccomp, MTE, PAC, BTI, KASLR) never validated. |

### 1.2 Critical Gap

**The repository has NOT demonstrated a naturally schedulable vulnerability.** Every race-triggering experiment (EXP-015, 018, 019, 022b, 023b) uses GDB to:
1. Break at hardcoded `__ep_remove+0x19c` (after `WRITE_ONCE`)
2. Patch Thread A's PC with `0x14000000` (infinite loop `b .`)
3. Manually continue after Thread B completes `ep_free`

This creates an **artificial preemption point that does not exist** in a `PREEMPT_VOLUNTARY` kernel. The instruction gap between `WRITE_ONCE(file->f_ep, NULL)` and `hlist_del_rcu` is ~20 cycles — too narrow for natural scheduler intervention without explicit `cond_resched`.

### 1.3 Phase 3 Mandate

Produce statistically valid evidence answering exactly one question:

> **Can the epoll UAF race be triggered on an unmodified Android GKI kernel without debugger intervention?**

If YES → Proceed to Android PoC development.
If NO → Document why the vulnerability is not practically exploitable on this configuration.

---

## 2. Dependency Graph: What Collapses If Natural Schedulability Fails

```
┌─────────────────────────────────────────────────────────────────┐
│  ASSUMPTION: Race is naturally reachable on unmodified kernel   │
└─────────────────────────────────────────────────────────────────┘
                              │
              ┌───────────────┼───────────────┐
              ▼               ▼               ▼
      ┌─────────────┐ ┌─────────────┐ ┌─────────────┐
      │  VER-026    │ │  VER-027    │ │  VER-028    │
      │ Race exists │ │ msg_msg     │ │ Only        │
      │ (lockless   │ │ reclaims    │ │ hlist_del_  │
      │  bypass)    │ │ freed slot  │ │ rcu touches │
      └──────┬──────┘ └──────┬──────┘ └──────┬──────┘
             │               │               │
             ▼               ▼               ▼
      ┌─────────────────────────────────────────┐
      │  "Verified Primitive = NULL write at    │
      │  offset 160 of reclaimed kmalloc-192"   │
      └─────────────────────────────────────────┘
             │
       ┌─────┴─────┐
       ▼           ▼
┌─────────────┐ ┌─────────────┐
│  VER-031/032│ │   VER-033   │
│ Chain 2     │ │ Dual-watch  │
│ dead        │ │ mutually    │
└─────────────┘ │ exclusive   │
                └─────────────┘
                        │
                        ▼
            ┌───────────────────────┐
            │ "Only DoS is possible" │
            └───────────────────────┘
```

**If natural schedulability is never demonstrated:**
- VER-026 (race existence) → **COLLAPSES** (only proven with GDB patch)
- VER-027 (msg_msg reclaim) → **COLLAPSES** (only works with GDB 2-3s window)
- VER-028/031/032 (primitive characterization) → **COLLAPSE** (only observed under debugger)
- VER-033 (mutual exclusion) → **PARTIAL** (EXP-024 used no patching, but still debugger-observed)
- **"Only DoS" conclusion** → **UNFOUNDED** (no evidence primitive is reachable at all)

---

## 3. Experiment Priority Queue

Ranked by: **(Expected Information Gain × Android Relevance) / Engineering Cost**

| Rank | Experiment ID | Question | EIG | Android Rel | Cost | Score | Status |
|------|---------------|----------|-----|-------------|------|-------|--------|
| **1** | **NAT-001** | Can race trigger without GDB? (Statistical) | 10 | 10 | 3 | **33.3** | PLANNED |
| **2** | **NAT-002** | Does race window exist at `cond_resched` in `ep_unregister_pollwait`? | 9 | 9 | 2 | **40.5** | PLANNED |
| **3** | **AND-001** | Does `msgsnd`/`msgrcv` work under Android seccomp? | 9 | 10 | 1 | **90.0** | PLANNED |
| **4** | **AND-002** | KASLR impact on race reliability & spray? | 8 | 10 | 2 | **40.0** | PLANNED |
| **5** | **AND-003** | SELinux policy blocks on exploit syscalls? | 7 | 10 | 1 | **70.0** | PLANNED |
| **6** | **NAT-003** | msg_msg reclaim reliability under natural timing? | 8 | 10 | 3 | **26.7** | PLANNED |
| **7** | **NAT-004** | Per-CPU partial slab interference on reclaim? | 6 | 8 | 2 | **24.0** | PLANNED |
| **8** | **AND-004** | MTE/PAC/BTI impact on UAF primitive? | 5 | 10 | 4 | **12.5** | PLANNED |
| **9** | **NAT-005** | Alternative spray if msg_msg blocked (add_key, setxattr)? | 7 | 9 | 3 | **21.0** | PLANNED |

---

## 4. Experiment Specifications

### 4.1 NAT-001: Statistical Natural Race Test

**Goal**: Measure natural race hit rate without debugger assistance.

**Hypothesis**: The race can be triggered naturally with hit rate > 0 on PREEMPT_VOLUNTARY kernel.

**Preconditions**:
- Kernel: `linux-6.12.67` (commit `7e35917775b8`), `PREEMPT_VOLUNTARY=y`, `CONFIG_SLUB_CPU_PARTIAL=y`
- Config: `kasan=off` (for now), `nokaslr` (for address stability), `nokaslr` removable later
- 2-CPU QEMU virt, `cortex-a57`, 2GB RAM

**Method**:
1. Build standalone harness (`test_nat001.c`) — see Appendix A
2. Harness runs N iterations per boot (fork-per-iteration for isolation)
3. Each iteration:
   - Thread A (CPU 0): `close(outer_epoll)` → busy-spin on atomic flag
   - Thread B (CPU 1): busy-spin on flag → `close(inner_epoll)` → msg_msg spray
   - Parent monitors child for SIGKILL/SIGSEGV (kernel OOPS)
4. Run 10,000 iterations minimum (statistical significance for p < 10⁻⁴)

**Evidence Required**:
- `tier2/evidence/NAT-001_raw_*.log` — raw harness output per boot
- `tier2/evidence/NAT-001_RESULTS.md` — hit count, iterations, 95% CI
- Kernel OOPS backtrace for any hit (must show `hlist_del_rcu` / `__ep_remove`)

**Success Criteria**: ≥1 hit in 10,000 iterations (p < 0.01 if true rate = 0)
**Failure Criteria**: 0 hits in 10,000 iterations → natural race probability < 10⁻⁴ (95% CI)

**Confidence Target**: High if hit rate ≥ 10⁻³; Medium if 10⁻⁴–10⁻³; Low if 0.

---

### 4.2 NAT-002: Preemption Point Analysis

**Goal**: Identify if/where natural preemption can occur in the race window.

**Hypothesis**: The only viable preemption point is the `cond_resched()` in `ep_unregister_pollwait` loop (line 888 of `ep_clear_and_put`), not inside `__ep_remove`.

**Preconditions**: Same as NAT-001 + `CONFIG_DEBUG_PREEMPT=y` (if available) or tracepoints.

**Method**:
1. Static analysis: Audit `__ep_remove` (lines 804-859) for `cond_resched`, `might_resched`, `preempt_enable` — **NONE exist**.
2. Audit `ep_clear_and_put` (lines 870-909): `cond_resched` at line 888 inside `for (rbp = rb_first_cached...)` loop calling `ep_unregister_pollwait`.
3. Dynamic tracing: Add kernel tracepoint at `ep_unregister_pollwait` entry/exit; measure time spent.
4. Test harness modification: Add explicit `sched_yield()` in Thread A between `ep_unregister_pollwait` and `WRITE_ONCE` to simulate preemption.

**Evidence Required**:
- Source code audit excerpt with line numbers
- Trace data showing `ep_unregister_pollwait` duration
- Hit rate comparison: with vs without explicit yield

**Success Criteria**: Race triggers reliably when Thread A yields at `ep_unregister_pollwait` exit.
**Failure Criteria**: Even with explicit yield, race window too narrow.

**Confidence**: High (source audit is deterministic).

---

### 4.3 NAT-003: msg_msg Reclaim Under Natural Timing

**Goal**: Measure msg_msg reclaim success rate when spray window is not GDB-controlled.

**Hypothesis**: msg_msg reclaim works naturally but with lower reliability due to `SLUB_CPU_PARTIAL` and background allocations.

**Method**:
1. Use NAT-001 harness but with spray active during entire race window
2. Vary spray thread count (1, 2, 4) and CPU affinity
3. Measure: (a) reclaim success (exact address match), (b) partial reclaim (same slab, different offset), (c) failure
4. Test with `slab_nomerge` and `slub_debug=F` boot params to isolate effects

**Evidence Required**:
- Reclaim statistics: exact match %, partial %, fail %
- `/proc/slabinfo` snapshots during race
- Correlation with hit rate from NAT-001

**Success Criteria**: Exact address reclaim ≥ 10% of successful race hits.
**Failure Criteria**: Reclaim only works with GDB's 2-3 second controlled window.

---

### 4.4 AND-001: SysV IPC Under Android Seccomp

**Goal**: Verify `msgget`/`msgsnd`/`msgrcv` are not blocked by Android's seccomp profile.

**Hypothesis**: SysV IPC is available on AVD (used by system services) but may be blocked for untrusted app context.

**Method**:
1. Build minimal test binary (`test_ipc.c`) calling `msgget(IPC_PRIVATE, 0666)`, `msgsnd()`, `msgrcv()`
2. Run on AVD as `shell` user (adb shell) and as untrusted app context (via `run-as`)
3. Check return codes: `ENOSYS` = seccomp blocked, `EACCES` = SELinux blocked
4. If blocked, test alternatives: `add_key`/`keyctl` (`CONFIG_KEYS=y`), `setxattr` (`CONFIG_TMPFS_XATTR=y`)

**Evidence Required**:
- Binary output showing syscall results
- `dmesg` for seccomp/SELinux denials
- Alternative spray validation if primary blocked

**Success Criteria**: `msgsnd`/`msgrcv` succeed in target context (shell or app).
**Failure Criteria**: All SysV IPC blocked → must pivot to `add_key` or `setxattr`.

**Android Relevance**: CRITICAL — if msg_msg blocked, entire spray strategy fails.

---

### 4.5 AND-002: KASLR Impact on Race & Spray

**Goal**: Measure how KASLR affects race reliability and spray targeting.

**Hypothesis**: KASLR adds entropy to slab addresses but does not prevent reclaim (freed slot address is known via race).

**Method**:
1. Rebuild kernel with `CONFIG_RANDOMIZE_BASE=y` (remove `nokaslr` from cmdline)
2. Run NAT-001 harness with KASLR enabled
3. Measure: (a) hit rate change, (b) spray address prediction accuracy
4. Note: Freed slot address is learned dynamically during race (GDB or crash analysis), not predicted statically

**Evidence Required**:
- Hit rate comparison: KASLR on vs off
- Entropy measurement: bits of randomness in `kmalloc-192` base

**Success Criteria**: Hit rate within 2x of KASLR-off baseline.
**Failure Criteria**: KASLR reduces hit rate to 0.

---

### 4.6 AND-003: SELinux Policy Analysis

**Goal**: Map SELinux denials for all exploit-relevant operations.

**Hypothesis**: SELinux enforcing may block `epoll` syscalls, `msgget`, `/dev/snd/timer`, netlink, etc.

**Method**:
1. Boot AVD with `androidboot.selinux=enforcing` (default)
2. Run test binaries for each primitive:
   - `epoll_create1`, `epoll_ctl`, `epoll_wait`
   - `msgget`, `msgsnd`, `msgrcv`
   - `open("/dev/snd/timer")`
   - netlink `RTM_NEWROUTE` (for `fib6_info`)
   - `add_key`, `keyctl`
   - `setxattr` on tmpfs
3. Capture `avc: denied` messages from `dmesg`
4. Test permissive mode (`androidboot.selinux=permissive`) as baseline

**Evidence Required**:
- SELinux denial log for each operation
- Policy module source if denials found (may need custom policy)

**Success Criteria**: All required primitives work in enforcing mode.
**Failure Criteria**: Critical primitive blocked → document workaround or prove impossible.

---

### 4.7 NAT-004: Per-CPU Partial Slab Interference

**Goal**: Quantify `SLUB_CPU_PARTIAL` impact on deterministic reclaim.

**Hypothesis**: Freed object goes to CPU-local partial slab; spray from different CPU may not reclaim it.

**Method**:
1. Run NAT-001 with Thread A and B pinned to SAME CPU vs DIFFERENT CPUs
2. Run with `slub_max_order=0` (force single-page slabs) vs default
3. Monitor `/proc/slabinfo` `partial` count for `kmalloc-192` during race
4. Test `echo 0 > /sys/kernel/slab/kmalloc-192/cpu_partial` (if available)

**Evidence Required**:
- Hit rate: same-CPU vs cross-CPU
- `slabinfo` partial count correlation

**Success Criteria**: Same-CPU reclaim works; cross-CPU fails predictably.
**Failure Criteria**: No measurable difference.

---

### 4.8 AND-004: Hardware Mitigations Impact

**Goal**: Assess MTE, PAC, BTI impact on UAF primitive.

**Hypothesis**: MTE tags cause crash on UAF access; PAC/BTI irrelevant for data-only primitive.

**Method**:
1. Rebuild kernel with `CONFIG_ARM64_MTE=y`, `CONFIG_KASAN_HW_TAGS=y`, `kasan=on`
2. Run NAT-001 harness (may crash earlier due to MTE)
3. Test if MTE tags on freed object cause synchronous crash on `hlist_del_rcu` write
4. Verify PAC/BTI not triggered (no control-flow hijack)

**Evidence Required**:
- Crash type: MTE tag fault vs NULL deref
- Whether MTE prevents exploitation entirely

**Success Criteria**: Race still triggers (MTE crash = detection, not prevention).
**Failure Criteria**: MTE makes race undetectable or changes primitive.

---

### 4.9 NAT-005: Alternative Spray Primitives

**Goal**: Validate backup spray if msg_msg blocked.

**Method**:
1. `add_key`/`keyctl`: Test payload sizes 129-192 bytes; check `/proc/slabinfo`
2. `setxattr` on tmpfs: Test value sizes; verify allocations persist
3. `io_uring`: If `CONFIG_IO_URING=y`, test buffer registration
4. Compare reclaim reliability vs msg_msg

**Evidence Required**:
- Spray success rate per primitive
- Slab cache placement verification

---

## 5. Android Portability Track

Maintain `ANDROID_PORTABILITY.md` (to be created) with:

| Linux Finding | Android Effect | PoC Impact | Remaining Blocker |
|---------------|----------------|------------|-------------------|
| Race requires GDB patch | **UNKNOWN** — may not work on device | **BLOCKER** | NAT-001/002 |
| msg_msg spray works | **UNKNOWN** — seccomp may block | **BLOCKER** | AND-001 |
| NULL write at offset 160 | Works if race works | Medium | NAT-001 |
| Chain 0/2 dead | Confirmed dead on Linux | Low | — |
| KASLR on | Adds entropy | Medium | AND-002 |
| SELinux enforcing | May block syscalls | **BLOCKER** | AND-003 |
| MTE/KASAN | May detect UAF early | Medium | AND-004 |
| PAC/BTI | Irrelevant (data-only) | None | — |

---

## 6. Statistical Standards

| Metric | Requirement |
|--------|-------------|
| Minimum iterations | 10,000 per configuration |
| Confidence level | 95% (Wilson score interval) |
| Reporting | Hit count, total iterations, hit rate, 95% CI |
| Reproducibility | Same kernel commit, config, QEMU version, harness binary hash |
| Environment recording | `uname -a`, `/proc/cmdline`, `/proc/config.gz` hash, GCC version |

---

## 7. Repository Hygiene Checklist

For every experiment:

- [ ] `tier2/docs/EXPERIMENT_INDEX.md` updated with RUNNING status BEFORE start
- [ ] `tier2/evidence/EXP-NNN_raw_*.log` committed
- [ ] `tier2/evidence/EXP-NNN_RESULTS.md` committed
- [ ] `tier2/scripts/exp_NNN_*.c/.py/.sh` committed
- [ ] `tier2/docs/VERIFICATION_LEDGER.md` updated with new VER-NNN entries
- [ ] `tier2/docs/ASSUMPTIONS_REGISTER.md` updated (new assumptions, status changes)
- [ ] `tier2/docs/UNKNOWNS_REGISTER.md` updated
- [ ] `tier2/docs/DEAD_ENDS_REGISTER.md` updated (if path permanently closed)
- [ ] `tier2/docs/ANDROID_PORTABILITY.md` updated
- [ ] `git status` clean, `git push`, `git ls-remote` verified

---

## 8. Execution Order & Dependencies

```
Week 1:
├── AND-001 (SysV IPC on AVD) — 1 day
│   └── If blocked → NAT-005 (alternative spray) in parallel
├── NAT-002 (Preemption point audit) — 1 day (static + trace)
├── NAT-001 (Statistical race test) — 3-5 days (10k iterations)
│   └── Run on KASLR-off first, then AND-002 (KASLR-on)
├── AND-003 (SELinux audit) — 1 day
└── NAT-003 (msg_msg reclaim stats) — 2 days (subset of NAT-001 runs)

Week 2:
├── NAT-004 (CPU partial interference) — 1 day
├── AND-004 (MTE/PAC/BTI) — 2 days (kernel rebuild + test)
├── AND-001 followup (if msg_msg blocked, NAT-005)
└── Integration: Full Android config test (KASLR+SELinux+MTE)

Go/No-Go Decision Point:
├── If NAT-001 hit rate > 0 → Proceed to Android PoC development
├── If NAT-001 hit rate = 0 AND NAT-002 shows no viable preemption → Document as not naturally exploitable
└── If NAT-001 hit rate = 0 BUT NAT-002 shows preemption with yield → Optimize timing, re-test
```

---

## 9. Appendix A: NAT-001 Harness Specification

```c
// test_nat001.c — Statistical Natural Race Test
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdatomic.h>
#include <string.h>

#define ITERATIONS_PER_BOOT 1000  // Parent runs multiple boots

int ep_outer, ep_inner;
atomic_int ready_a = 0, ready_b = 0, go = 0;

void *thread_a(void *arg) {
    cpu_set_t cs; CPU_ZERO(&cs); CPU_SET(0, &cs);
    pthread_setaffinity_np(pthread_self(), sizeof(cs), &cs);
    
    atomic_store_explicit(&ready_a, 1, memory_order_release);
    while (!atomic_load_explicit(&go, memory_order_acquire)) { /* spin */ }
    
    close(ep_outer);  // Triggers __ep_remove
    return NULL;
}

void *thread_b(void *arg) {
    cpu_set_t cs; CPU_ZERO(&cs); CPU_SET(1, &cs);
    pthread_setaffinity_np(pthread_self(), sizeof(cs), &cs);
    
    atomic_store_explicit(&ready_b, 1, memory_order_release);
    while (!atomic_load_explicit(&go, memory_order_acquire)) { /* spin */ }
    
    // msg_msg spray
    int msgq = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);
    struct { long mtype; char mtext[144]; } msg = { .mtype = 1 };
    memset(msg.mtext, 0x41, 144);
    for (int i = 0; i < 5000; i++) {
        if (msgsnd(msgq, &msg, 144, IPC_NOWAIT) < 0) break;
    }
    
    close(ep_inner);  // Triggers eventpoll_release -> ep_free
    msgctl(msgq, IPC_RMID, NULL);
    return NULL;
}

int run_one_iteration() {
    pid_t pid = fork();
    if (pid == 0) {
        // Child: isolated address space
        ep_outer = epoll_create1(0);
        ep_inner = epoll_create1(0);
        struct epoll_event ev = { .events = EPOLLIN, .data.fd = ep_inner };
        epoll_ctl(ep_outer, EPOLL_CTL_ADD, ep_inner, &ev);
        
        pthread_t ta, tb;
        pthread_create(&ta, NULL, thread_a, NULL);
        pthread_create(&tb, NULL, thread_b, NULL);
        
        while (!atomic_load_explicit(&ready_a, memory_order_acquire) ||
               !atomic_load_explicit(&ready_b, memory_order_acquire)) { }
        
        atomic_store_explicit(&go, 1, memory_order_release);
        
        pthread_join(ta, NULL);
        pthread_join(tb, NULL);
        _exit(0);  // Clean exit = no race hit
    }
    
    // Parent: wait for child, check for crash
    int status;
    waitpid(pid, &status, 0);
    
    if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        if (sig == SIGKILL || sig == SIGSEGV || sig == SIGBUS) {
            return 1;  // Kernel OOPS killed child
        }
    }
    return 0;  // No crash
}

int main() {
    printf("[NAT-001] Statistical Natural Race Test\n");
    printf("[NAT-001] Target: 10,000 iterations\n");
    
    int hits = 0, total = 0;
    for (int boot = 0; boot < 10; boot++) {  // 10 boots × 1000 = 10,000
        for (int i = 0; i < ITERATIONS_PER_BOOT; i++) {
            atomic_store(&ready_a, 0);
            atomic_store(&ready_b, 0);
            atomic_store(&go, 0);
            
            int r = run_one_iteration();
            if (r == 1) { hits++; printf("[!] HIT #%d at iter %d\n", hits, total); }
            total++;
            
            if (total % 1000 == 0) {
                printf("[*] Progress: %d/%d, hits=%d (%.6f%%)\n", 
                       total, 10000, hits, 100.0 * hits / total);
            }
        }
        // Reboot QEMU between boots to reset kernel state
        printf("[*] Boot %d complete. Hits: %d/%d\n", boot+1, hits, total);
    }
    
    printf("[NAT-001] FINAL: %d hits in %d iterations (%.6f%%)\n", 
           hits, total, 100.0 * hits / total);
    // Calculate Wilson 95% CI
    return hits > 0 ? 0 : 1;
}
```

---

## 10. Appendix B: Key Source References

| Function | File | Lines | Relevance |
|----------|------|-------|-----------|
| `__ep_remove` | `fs/eventpoll.c` | 804-859 | UAF operations |
| `ep_clear_and_put` | `fs/eventpoll.c` | 870-909 | Preemption point at line 888 |
| `ep_unregister_pollwait` | `fs/eventpoll.c` | ~740 | Called in loop with `cond_resched` |
| `eventpoll_release` | `include/linux/eventpoll.h` | 34-54 | Lockless fast path |
| `ep_free` | `fs/eventpoll.c` | 788-794 | Frees `struct eventpoll` |
| `load_msg` | `ipc/msgutil.c` | 95 | msg_msg allocation |
| `hlist_del_rcu` | `include/linux/rculist.h` | 516 | NULL write at offset 160 |

---

## 11. Sign-Off

This plan represents the adversarial review's assessment of the minimum viable path to scientifically defensible characterization of CVE-2026-46242 on Android GKI.

**Next Action**: Execute NAT-002 (static preemption audit) and AND-001 (SysV IPC test) in parallel as they require no statistical runs and unblock all subsequent work.

**Commitment**: All experiments will follow the Evidence Protocol (Rule 1-10) and produce committed artifacts before any conclusion is claimed.