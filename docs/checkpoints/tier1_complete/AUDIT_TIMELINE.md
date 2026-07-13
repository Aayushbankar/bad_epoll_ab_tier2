# Phase 4: Engineering Timeline

## Stage 1: Initial Compilations and Library Linking
- **Problem**: Attempting to compile `exploit.cpp` returned undefined references to `libkernelXDK` internal methods and structures (e.g., `mono_ns` wrappers).
- **Investigation**: Verified the `libxdk` library headers and objects. Noticed `exploit.cpp` namespace mapping was slightly out of sync with recent library refactoring.
- **Solution**: Patched the local `Makefile` and `exploit.cpp` logic to properly link `-lkernelXDK`, satisfying the symbol table.
- **Knowledge Gained**: The exploit is entirely dependent on `libxdk` for managing UAF states, allocating payloads, and mapping arbitrary structs. 

## Stage 2: First Execution and the UAF Trigger
- **Problem**: Exploits ran but immediately panic'd without printing expected userspace validation (`Win!`).
- **Investigation**: Captured standard QEMU output logging, which verified the Arbitrary Address Read (AAR) successfully bypassed KASLR (identified the active `task_struct` and `init_task.comm=swapper`).
- **Knowledge Gained**: The actual UAF bug was triggering successfully, meaning the vulnerability primitive was completely stable. The failure occurred during the `rip_post_exploit` transition phase.

## Stage 3: The JOP Pivot Hallucination
- **Problem**: Kernel panic explicitly printed an invalid RIP of `0xffffffff810001bd`. A disassembly of that address showed it was *not* the expected `pop rdi` instruction.
- **Investigation**: Analyzed the JOP stack pivot chain (PIVOT1 through PIVOT4) connecting the vulnerable `f_op->poll` execution to the custom memory page. Initial hypothesis incorrectly assumed `objdump` AT&T syntax confused the register assignments of `PIVOT4` (`push %rdx` vs `push %rdi`).
- **Solution**: Patched the database manually for hours in a flawed attempt to replace `PIVOT4`.
- **Knowledge Gained**: Do not optimize or guess assembly instructions without runtime validation. AT&T syntax vs Intel syntax discrepancies often misdirect analysis.

## Stage 4: Runtime Provenance Audit
- **Problem**: Required definitive proof that the JOP stack bridges were not the cause of the `0x1bd` offset panic.
- **Evidence**: Created `trace_gdb.py` to freeze the QEMU environment instantly at `f_op->poll` invocation and track CPU register states instruction-by-instruction.
- **Solution**: Discovered the JOP stack bridge worked flawlessly. The exact payload deposited onto the heap by `exploit.cpp` was verified via raw memory dump, proving the invalid `0x1bd` gadget originated directly from the underlying database object, not the parser.
- **Files Changed**: Generated `gdb_trace.log` and `gdb_trace_dump.log`.

## Stage 5: Database Regeneration
- **Problem**: The shipped `target_db.kxdb` was compiled for the generic kernelCTF image, causing significant `.text` misalignment when running against the locally compiled Fedora GCC `vmlinux`.
- **Investigation**: Conducted an end-to-end repository audit and discovered the `image_db` generator pipeline consisting of `angrop`, `rp++`, and Python structural scrapers.
- **Solution**: Resolved a `PicklingError` inside `angrop_rop_generator.py` by patching `angrop/rop_utils.py` natively. Executed the pipeline to regenerate `rop_actions.json`, extracting the correct `pop rdi; ret` offset (`0x6fe9`). Compiled the fresh database utilizing `kxdb_tool`.
- **Files Changed**: `target_db.kxdb`, `rop_utils.py` (angrop dependency).

## Stage 6: The Final Shell
- **Problem**: After successfully swapping databases, the kernel panicked with "Attempted to kill init!" following a successful exploit execution.
- **Investigation**: Evaluated `qemu_output.log` which outputted `Win! UID: 0, GID: 0, EUID: 0`. The exploit *did* succeed, however the transition to `/bin/bash` failed because it didn't exist in the initramfs, and `/bin/sh` subsequently reached EOF instantly in the non-interactive background QEMU execution.
- **Solution**: Reconfigured `rip_post_exploit` in `exploit.cpp` to natively run `/bin/sh -c "id; uname -a; echo ROOT_SHELL_SUCCESS"`, providing non-interactive evidence of complete system compromise.
- **Knowledge Gained**: Exploit processes overriding PID 1 within an `initramfs` will trigger immediate system panics if the designated userspace shell process lacks an active pseudo-terminal or reaches EOF natively.
