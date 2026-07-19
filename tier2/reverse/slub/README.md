# SLUB Allocator Research Notebook

## Purpose
Analyze the SLUB allocator configuration in Android GKI (`CONFIG_SLAB_FREELIST_RANDOM`, `CONFIG_SLAB_FREELIST_HARDENED`) and its impact on predictable heap layouts.

## Important Source Files
- `mm/slub.c`

## Important Structures
- `struct kmem_cache`
- `struct page` / `struct slab`

## Important Functions
- `kmem_cache_alloc()`
- `kmem_cache_free()`

## Execution Flow
*(To be populated after source sync and GDB tracing)*

## Memory Layout
- Heap randomization significantly impacts contiguous allocations.

## Exploitation Relevance
The UAF object (`epitem`) resides in a dedicated slab cache (`eventpoll_epi`). Cross-cache exploitation techniques (e.g., triggering `kmalloc` allocations to overlap with `eventpoll_epi` boundaries) are heavily mitigated by SLUB hardening.

## Open Questions
- Can we reliably overlap `kmalloc-128` (or similar) with `eventpoll_epi` on GKI?
- Does `CONFIG_SLAB_FREELIST_HARDENED` prevent simple pointer overwrites in the freelist?

## Notes
- None yet.

## References
- None yet.
