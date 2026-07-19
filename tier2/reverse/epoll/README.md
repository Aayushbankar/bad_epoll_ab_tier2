# epoll Subsystem Research Notebook

## Purpose
Investigate the `fs/eventpoll.c` implementation within Android 14 GKI (Linux 6.1) to understand event notification lifecycles, locking mechanisms, and the root cause of the CVE-2026-46242 Use-After-Free.

## Important Source Files
- `fs/eventpoll.c`
- `include/linux/eventpoll.h`

## Important Structures
- `struct eventpoll`
- `struct epitem`
- `struct eppoll_entry`

## Important Functions
- `ep_insert()`
- `ep_remove()`
- `ep_poll()`
- `ep_free()`

## Execution Flow
*(To be populated after source sync and GDB tracing)*

## Memory Layout
- Slab cache: `eventpoll_epi` and `eventpoll_pwq`
- Object size: Unknown (Requires SLUB analysis on ARM64)

## Exploitation Relevance
Provides the fundamental Use-After-Free object (`epitem`) that enables Arbitrary Read/Write in CVE-2026-46242.

## Open Questions
- Does GKI `fs/eventpoll.c` have any backported stability fixes preventing the UAF trigger?
- What are the exact offsets of `ffd`, `rdllink`, and `next` within `struct epitem` on ARM64 GKI?

## Notes
- None yet.

## References
- None yet.
