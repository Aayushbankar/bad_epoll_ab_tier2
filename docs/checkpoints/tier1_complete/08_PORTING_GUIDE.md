# Porting Guide

This document describes the workflow for successfully porting the `CVE-2026-46242` Tier 1 exploit to a newly compiled kernel version or a completely separate architecture/build.

## Unchanging Constants
- The fundamental C++ source code in `exploit.cpp` and `libxdk` does not need to be manually edited to support standard offset shifts.
- The Use-After-Free timing constraints and core `task_struct` leak primitives generally remain identical across point releases.

## The Generation Pipeline
1. Compile the target `vmlinux` binary with debug symbols (DWARF required).
2. Create a new directory within `image_db/releases/` formatted precisely as `<vendor>/<version>` (e.g., `fedora/6.12.67`).
3. Deposit `vmlinux` and the compressed `vmlinuz` directly into the release folder.
4. Execute the Python generator scripts in order:
   - Extract `.text` symbols and `BTF` layout.
   - Run DWARF structure scraping for `task_struct`, `cred`, etc.
   - Boot `angrop_rop_generator.py` and `pivot_finder.py` using `rp++` to automatically locate valid ROP action offsets and JOP stack bridges.
5. Compile the new `target_db.kxdb` using the `kxdb_tool` binary.

## Debugging and Validation
- Replace the root `target_db.kxdb` and utilize the `dump_rop_debug` tool to manually verify offset alignments against a disassembled snapshot of `vmlinux` using `objdump`.
- If the exploit triggers a kernel panic or supervisor fault on execution, utilize the Python GDB trace scripts (`trace_qemu.py`) directly from `exploit/tier1-linux-vm/` to verify the execution trace of the pivot chain up to `kernel_rop[0]`.
