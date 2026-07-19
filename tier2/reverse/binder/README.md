# Binder Research Notebook

## Purpose
Investigate the Android Binder IPC subsystem for potential use as a reliable allocation primitive, heap spraying mechanism, or alternative escalation vector.

## Important Source Files
- `drivers/android/binder.c`
- `drivers/android/binder_alloc.c`

## Important Structures
- `struct binder_proc`
- `struct binder_thread`
- `struct binder_node`
- `struct binder_transaction`

## Important Functions
- `binder_ioctl()`
- `binder_transaction()`
- `binder_thread_write()`

## Execution Flow
*(To be populated after source sync and GDB tracing)*

## Memory Layout
- Binder maps a specific `vm_area_struct` per process for receiving transactions.

## Exploitation Relevance
Binder is the core IPC mechanism on Android. It is frequently used by exploit developers to spray the heap, construct predictable object layouts, and interact with privileged daemon processes. 

## Open Questions
- Can Binder transactions be used to reliably spray controlled data adjacent to `eventpoll_epi` objects?
- Are there any new Android 14 Binder mitigations?

## Notes
- None yet.

## References
- None yet.
