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
| EXP-016 | 2026-07-31 | Target selection: filter all 129-192 byte structs for kmalloc-192 spray and offset 160 NULL write primitive. | (pending) | (pending) | (pending) | **RUNNING** | (pending) |
| EXP-018 | 2026-08-01 | msg_msg Spray Reclaim Verification | test_exp018.c | exp018_gdb.py | run_exp018.sh | **PASSED** (Reclaim verified) | `tier2/evidence/EXP-018_RESULTS.md` |
| EXP-019 | 2026-08-01 | Controlled Crash PoC (Chain 0) | test_exp019.c | exp019_gdb.py | run_exp019.sh | **FAILED** (percpu_counter_dec not on freed ep) | `tier2/evidence/EXP-019_RESULTS.md` |
| EXP-022b | 2026-08-01 | Info Leak via hlist_del_rcu (2 epitems) | test_exp022b.c | exp022b_gdb.py | run_exp022b.sh | **PASSED** (Kernel pointer leaked: 0xffff0000037bb6d0) | `tier2/evidence/EXP-022b_RESULTS.md` |
