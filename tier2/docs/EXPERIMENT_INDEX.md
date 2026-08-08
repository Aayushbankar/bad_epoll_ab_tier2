# Experiment Index

This index logs all test reproducers, GDB automation scripts, and experimental execution runs within the Tier2 research environment.

---

## Experiment Registry

| Experiment ID | Experiment Name | Primary Objective | Reproducer Binary | GDB Script | Execution Shell Launcher | Result Summary | Evidence Reference |
|---|---|---|---|---|---|---|---|
| EXP-006 | 2026-07-23 | Single-watch close-vs-close UAF on struct file (pipe_buffer cross-cache) | struct file f_ep | test_pipe_reclaim | run_gdb_pipe_reclaim_test.sh | INCONCLUSIVE | tier2/evidence/EXP-006_raw_gdb.log |

| EXP-007 | 2026-07-27 | Cache isolation verification + same-cache epitem reclaim | eventpoll_epi cache | test_epoll_spray | gdb_epoll_spray.py | **PASSED** (Reclaim verified) | tier2/evidence/EXP-007_raw_gdb.log |
| EXP-008 | 2026-07-28 | Timing analysis: does list_del_init corrupt freed or live reclaimed epitem? Primitive assessment. | eventpoll_epi cache | test_epoll_spray | gdb_epoll_spray.py (extended) | **PASSED** (Dead end identified) | tier2/evidence/EXP-008_raw_gdb.log |
| EXP-009 | 2026-07-28 | `struct file` UAF: Identify if `epi->ffd.file` is accessed after the underlying file's refcount drops to zero, and attempt pipe_buffer cross-cache reclaim of the file struct slot. | filp cache (struct file) | (pending) | (pending) | **PASSED** | (pending) |
| EXP-010 | 2026-07-29 | `struct file` UAF: Determine whether pipe_buffer allocations give content control over the reclaimed struct file | filp cache / pipe_buffer cache | (none) | (none) | **PASSED** (Dead end identified) | tier2/evidence/EXP-010_RESULTS.md |
| EXP-014 | 2026-07-30 | Investigate Thread B `eventpoll_release_file` path for UAF where a stale `epitem` is dereferenced after Thread A reclaims it. | epitem_spray_bin | gdb_uaf_trace.py | run_uaf_test.sh | **DISPROVED** | `tier2/evidence/EXP-014_RESULTS.md` |
| EXP-015 | 2026-07-31 | Hardware trace of two-threaded race bypassing `eventpoll_release_file` via lockless fast-path. | test_exp014.c | exp015_gdb.py | run_exp015.sh | **VERIFIED** | `tier2/evidence/EXP-015_unified_trace.log` |
| EXP-016 | 2026-07-31 | Target selection: filter all 129-192 byte structs for kmalloc-192 spray and offset 160 NULL write primitive. | (pahole + source audit) | (N/A) | (N/A) | **COMPLETED** (Clean negative: no viable target beyond DoS) | `tier2/docs/EXP-016_RESULTS.md` |
| EXP-018 | 2026-08-01 | msg_msg Spray Reclaim Verification | test_exp018.c | exp018_gdb.py | run_exp018.sh | **PASSED** (Reclaim verified) | `tier2/evidence/EXP-018_RESULTS.md` |
| EXP-019 | 2026-08-01 | Controlled Crash PoC (Chain 0) | test_exp019.c | exp019_gdb.py | run_exp019.sh | **FAILED** (percpu_counter_dec not on freed ep) | `tier2/evidence/EXP-019_RESULTS.md` |
| EXP-022b | 2026-08-01 | Info Leak via hlist_del_rcu (2 epitems) | test_exp022b.c | exp022b_gdb.py | run_exp022b.sh | **RETRACTED** (See EXP-024: race conditions mutually exclusive) | `tier2/evidence/EXP-022b_RESULTS.md` |
| EXP-023b | 2026-08-01 | percpu_counter_dec Arbitrary Decrement Test (GDB-assisted) | test_exp023b.c | exp023b_gdb.py | run_exp023b.sh | **INCONCLUSIVE** (Redirect failed - ep param is outer epoll) | `tier2/evidence/EXP-023b_RESULTS.md` |
| EXP-024 | 2026-08-02 | Clean re-test of dual-watch KASLR leak (VER-029/030 retraction) | test_exp024.c | exp024_gdb.py | run_exp024.sh | **PASSED** (Negative: write to LIVE memory, not UAF. VER-029/030 retracted.) | `tier2/evidence/EXP-024_RESULTS.md` |

---

## Phase 3: Natural Schedulability & Android Portability

| Experiment ID | Experiment Name | Primary Objective | Reproducer Binary | GDB Script | Execution Shell Launcher | Result Summary | Evidence Reference |
|---|---|---|---|---|---|---|---|
| NAT-001 | 2026-08-02 | Statistical Natural Race Test (10k iterations, no GDB) | test_nat001.c | (none) | run_nat001.sh | **FAILED** (0/10,000 hits; race not naturally winnable) | `tier2/evidence/NAT-001/NAT-001_RESULTS.md` |
| NAT-002 | 2026-08-02 | Preemption Point Audit: cond_resched at lines 888/903 — CORRECTED: no-op in 2-CPU pinned PREEMPT_VOLUNTARY | test_nat002.c | exp_nat002_trace.py | run_nat002.sh | **CORRECTED** (cond_resched no-op; timing window only) | `tier2/evidence/NAT-002/NAT-002_RESULTS.md` |
| NAT-003 | 2026-08-02 | msg_msg Reclaim Under Natural Timing | test_nat001.c (subset) | (none) | run_nat001.sh | **PLANNED** | `tier2/evidence/NAT-003_raw_*.log` |
| NAT-004 | 2026-08-02 | Per-CPU Partial Slab Interference | test_nat004.c | (none) | run_nat004.sh | **PLANNED** | `tier2/evidence/NAT-004_raw_*.log` |
| NAT-005 | 2026-08-05 | Adaptive Launch-Ahead Search & Topology Verification | test_nat005.c | exp_nat005_gdb.py | run_nat005.sh | **PASSED** (0/92,740 hits; isolcpus=1 + 4MB eviction; best alignment error=1 cycle) | `tier2/evidence/NAT-005_RESULTS.md` |
| AND-001 | 2026-08-05 | SysV IPC Availability Under Android Target | test_and001.c | exp_and001_gdb.py | run_and001.sh | **PASSED** (SysV IPC functional; load_msg trapped) | `tier2/evidence/AND-001_raw_ipc.log` |
| AND-002 | 2026-08-08 | KASLR Impact on Race Reliability (NAT-005 harness, KASLR on vs off) | test_nat005.c | exp_and002_gdb.py | run_and002.sh | **RUNNING** | `tier2/evidence/AND-002_raw_kaslr_off.log`, `tier2/evidence/AND-002_raw_kaslr_on.log` |
| AND-003 | 2026-08-02 | SELinux Policy Audit for Exploit Syscalls | test_and003.c | exp_and003_gdb.py | run_and003.sh | **PASSED** (All 6 syscalls allowed) | `tier2/evidence/AND-003_RESULTS.md`, `tier2/evidence/AND-003_raw_enforcing.log` |
| AND-004 | 2026-08-02 | MTE/KASAN_HW_TAGS Impact on UAF Primitive | test_nat001.c | (none) | run_and004.sh | **PLANNED** | `tier2/evidence/AND-004_raw_*.log` |
