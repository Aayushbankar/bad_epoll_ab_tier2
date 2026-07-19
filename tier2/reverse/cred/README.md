# cred Structure Research Notebook

## Purpose
Investigate the `struct cred` implementation in Android 14 GKI to identify the target structure for local privilege escalation (LPE) payload overwrites.

## Important Source Files
- `include/linux/cred.h`
- `kernel/cred.c`

## Important Structures
- `struct cred`

## Important Functions
- `commit_creds()`
- `prepare_creds()`

## Execution Flow
*(To be populated after source sync and GDB tracing)*

## Memory Layout
- Slab cache: `cred_jar`
- Object size: Unknown (Requires SLUB analysis on ARM64)

## Exploitation Relevance
The ultimate target for gaining root. Overwriting `uid`, `gid`, and capabilities array within a process's `cred` struct grants `init` level privileges.

## Open Questions
- Are `cred` structures isolated in dedicated SLUB caches (`CONFIG_RANDOMIZE_KSTACK_OFFSET_DEFAULT`, `CONFIG_SLAB_FREELIST_HARDENED`)?
- Does SELinux prevent direct modification of `cred` structs without triggering a panic?

## Notes
- None yet.

## References
- None yet.
