# Engineering Timeline

## Day 1: Project Initiation & Tier 1 Setup
- Analyzed the Tier 1 Linux VM QEMU setup and `bad-epoll-lab` repository structure.
- Resolved compilation issues in `exploit.cpp` and linked the `libxdk` framework to generate the baseline executable.
- Discovered that the exploit logic executes but panics exactly at the final RIP hijack stage (`__x86_indirect_call_thunk_rdi+0x5`).

## Day 2: Root Cause Investigation & Pivot Hallucination
- Re-analyzed the kernel panic and stack layout.
- Incorrectly hypothesized that the stack pivot instruction `PIVOT4` was misinterpreted by `objdump` due to AT&T vs. Intel syntax discrepancies.
- Spent time searching for alternative 1-gadget and 2-gadget stack pivots in the `vmlinux` binary.
- Manually patched the `.kxdb` file and `exploit.cpp` logic in a failed attempt to bypass the suspected bad pivot.

## Day 3: Forensic Auditing & Data Provenance
- Pivoted from ad-hoc patching to a strict forensic methodology.
- Attached a remote GDB trace script to log the exact execution flow from `PIVOT1` down to the final crash.
- Definitively proved via GDB logs that the `libxdk` payload parser and the existing JOP stack pivots (PIVOT1 through PIVOT4) were perfectly valid and successfully routed execution.
- Traced the `0xffffffff810001bd` crash address back to the pre-compiled `target_db.kxdb` database.
- Identified that `target_db.kxdb` was compiled against the official kernelCTF release binary, causing a desync with the local Fedora GCC-compiled `vmlinux`.

## Day 4: Knowledge Transfer & Checkpointing
- Overhauled the repository documentation to produce a 12-part `ENGINEERING_KT.md` transferring all architectural and forensic insights.
- Proceeded to fully regenerate the `target_db.kxdb` profile for the local `vmlinux` using `kxdb_tool` and `angrop`.
- Successfully validated that the new generated ROP gadgets aligned correctly with the local kernel layout (the new `pop rdi; ret` offset was `0x6fe9`).
- Executed the regenerated exploit, successfully bypassing the kernel panic and securing a root shell (`Win! UID: 0, GID: 0, EUID: 0`).
- Archived the fully functional Tier 1 state.
