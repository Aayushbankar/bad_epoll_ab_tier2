# Scheduler Research Notebook

## Purpose
Analyze the Linux scheduler in the Android GKI to identify timing determinism, preemption models, and mechanisms for manipulating exploit race conditions.

## Important Source Files
- `kernel/sched/core.c`
- `kernel/sched/fair.c`

## Important Structures
- `struct rq` (runqueue)
- `struct task_group`
- `struct sched_entity`

## Important Functions
- `schedule()`
- `preempt_schedule()`

## Execution Flow
*(To be populated after source sync and GDB tracing)*

## Memory Layout
- Per-CPU runqueues.

## Exploitation Relevance
CVE-2026-46242 relies on a Use-After-Free triggered by a race condition during parallel `epoll_ctl` and `close()` operations. The Android kernel's `CONFIG_PREEMPT` setting and scheduler granularity directly govern the success rate of this race window.

## Open Questions
- Is `CONFIG_PREEMPT` enabled on Android 14 GKI?
- Can we pin threads to specific CPUs reliably on this kernel configuration using `sched_setaffinity`?

## Notes
- None yet.

## References
- None yet.
