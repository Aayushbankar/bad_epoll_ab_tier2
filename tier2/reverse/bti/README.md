# BTI (Branch Target Identification) Research Notebook

## Purpose
Analyze ARMv8.5-A Branch Target Identification (BTI) within the Android GKI to understand its constraints on indirect jumps and control-flow hijacking.

## Important Source Files
- `arch/arm64/kernel/traps.c`
- `arch/arm64/kernel/entry.S`

## Important Structures
- PTE (Page Table Entry) configuration for BTI.

## Important Functions
- Exception handlers for `BTI` exceptions.

## Execution Flow
*(To be populated after source sync and GDB tracing)*

## Memory Layout
- Memory pages containing executable code are marked with the `Guarded` attribute (GP bit).

## Exploitation Relevance
Even if PAC is bypassed, BTI dictates that any indirect branch (`BR` or `BLR`) must land on a specific instruction (`BTI c`, `BTI j`, or `BTI jc`, or `PACIASP`). Arbitrary jumps into the middle of functions (like traditional ROP/JOP) will trigger an immediate exception.

## Open Questions
- Is BTI strictly enforced on all loaded kernel modules?
- Are there any known BTI-compliant gadgets ("BTI/PAC gadgets") suitable for JOP chains in GKI 6.1?

## Notes
- None yet.

## References
- None yet.
