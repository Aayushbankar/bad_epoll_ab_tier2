# task_struct Research Notebook

## Purpose
Analyze the `task_struct` layout in Android GKI to locate pointers to `cred` and thread info structures for data-only privilege escalation.

## Important Source Files
- `include/linux/sched.h`

## Important Structures
- `struct task_struct`
- `struct thread_info`

## Important Functions
- `find_task_by_vpid()`

## Execution Flow
*(To be populated after source sync and GDB tracing)*

## Memory Layout
- Slab cache: `task_struct`
- Object size: Unknown (Requires SLUB analysis on ARM64)

## Exploitation Relevance
Locating the current process's `task_struct` allows tracing the pointer to the `cred` structure. It is heavily utilized in data-only arbitrary read/write exploits.

## Open Questions
- What is the exact offset of `cred` within `task_struct` in Android 14 GKI? (Android adds heavily to this struct).
- Can we leak the `task_struct` address deterministically?

## Notes
- None yet.

## References
- None yet.
