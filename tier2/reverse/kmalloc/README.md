# kmalloc Research Notebook

## Purpose
Understand the generic memory allocation primitives used throughout the Android kernel for exploitation spraying and object overlapping.

## Important Source Files
- `include/linux/slab.h`

## Important Structures
- generic `kmalloc` caches

## Important Functions
- `kmalloc()`
- `kzalloc()`
- `kfree()`

## Execution Flow
*(To be populated after source sync and GDB tracing)*

## Memory Layout
- Generic caches (`kmalloc-32`, `kmalloc-64`, `kmalloc-128`, etc.)

## Exploitation Relevance
Used to allocate controlled data adjacent to vulnerable objects or to re-allocate freed objects (UAF overlapping).

## Open Questions
- What objects reside in the same cache as `eventpoll_epi`?
- Are `kmalloc` caches merged with dedicated caches on GKI?

## Notes
- None yet.

## References
- None yet.
