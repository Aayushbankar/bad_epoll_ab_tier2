# Experiment Index

This index logs all test reproducers, GDB automation scripts, and experimental execution runs within the Tier2 research environment.

---

## Experiment Registry

| Experiment ID | Experiment Name | Primary Objective | Reproducer Binary | GDB Script | Execution Shell Launcher | Result Summary | Evidence Reference |
|---|---|---|---|---|---|---|---|
| **EXP-001** | `snd_timer_reachability` | Validate userspace reachability of `/dev/snd_timer` | `tier2/reproducers/test_snd_timer.c` | N/A | Direct init execution | Successful open (`fd=3`) | [VER-001](file:///mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/docs/VERIFICATION_LEDGER.md#VER-001) |
| **EXP-002** | `epoll_snd_timer_reuse` | Prove exact address reuse between `inner_epoll` and `snd_timer_user` | `tier2/reproducers/test_reuse.c` | `tier2/scripts/gdb_reuse_test.py` | `tier2/scripts/run_gdb_reuse_test.sh` | Confirmed reuse at `0xffffff8003beb480` | [VER-002](file:///mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/docs/VERIFICATION_LEDGER.md#VER-002) |
| **EXP-003** | `single_watch_stale_write` | Observe stale store in single-watch topology | `tier2/reproducers/test_timer_write.c` | `tier2/scripts/gdb_timer_write_test.py` | `tier2/scripts/run_gdb_timer_write_test.sh` | Confirmed store `str x2, [x0]`, `x2 == 0` | [VER-004](file:///mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/docs/VERIFICATION_LEDGER.md#VER-004) |
| **EXP-004** | `dual_watch_stale_write` | Observe stale store in dual-watch topology | `tier2/reproducers/test_timer_write.c` | `tier2/scripts/gdb_timer_write_test.py` | `tier2/scripts/run_gdb_timer_write_test.sh` | Confirmed non-NULL store (`x2 == &epi1`) | [VER-006](file:///mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/docs/VERIFICATION_LEDGER.md#VER-006) |
| **EXP-005** | `wait_list_corruption_trace` | Measure `wait_list` next/prev before and after store | `tier2/reproducers/test_timer_write.c` | `tier2/scripts/gdb_timer_write_test.py` | `tier2/scripts/run_gdb_timer_write_test.sh` | Observed `next` changed to `&epi1`, `prev` remaining `NULL` | [VER-007](file:///mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/docs/VERIFICATION_LEDGER.md#VER-007) |

---

## Experiment Registry Guidelines

1. **Naming Standard**: Every experiment binary must be placed in `tier2/reproducers/` and prefixed with `test_`.
2. **GDB Script Alignment**: GDB automation scripts must be placed in `tier2/scripts/` and match the reproducer target.
3. **Execution Script Requirement**: Every experiment must have a corresponding shell script launcher in `tier2/scripts/run_gdb_*.sh`.
