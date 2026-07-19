# PAC (Pointer Authentication) Research Notebook

## Purpose
Investigate ARMv8.3-A Pointer Authentication Codes (PAC) implementation in the Android GKI to understand how function pointers are protected and discover potential bypasses.

## Important Source Files
- `arch/arm64/kernel/pointer_auth.c`
- `arch/arm64/include/asm/pointer_auth.h`

## Important Structures
- PAC keys in `task_struct`

## Important Functions
- `ptrauth_keys_init()`
- `ptrauth_thread_init_kernel()`

## Execution Flow
*(To be populated after source sync and GDB tracing)*

## Memory Layout
- PAC signatures occupy the upper bits of a 64-bit pointer.

## Exploitation Relevance
The Tier 1 exploit relied on overwriting `f_op->poll` (a function pointer). PAC absolutely prevents this. A forged pointer without a valid PAC signature triggers an instruction abort exception during execution.

## Open Questions
- Can we leak the PAC keys from the kernel?
- Are data pointers protected (PACD) or only instruction pointers (PACI)?
- Can we perform a data-only attack to bypass the need for PAC forgery entirely?

## Notes
- None yet.

## References
- None yet.
