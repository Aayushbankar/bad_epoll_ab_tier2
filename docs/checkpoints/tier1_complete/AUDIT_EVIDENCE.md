# Phase 3: Runtime Evidence

This document catalogs every runtime artifact generated during the engineering and validation of the Tier 1 exploit. All files referenced are securely archived in the top-level `artifacts/` folder, backed by `SHA256SUMS`.

## Generated Binaries
- **`vmlinux` & `bzImage`**: (Essential) The locally compiled vulnerable Linux 6.12.67 kernel objects. Sourced from the GCC compilation pipeline.
- **`exploit`**: (Essential) The statically linked `exploit.cpp` binary that triggers the UAF and injects the JOP/ROP payload.
- **`initramfs_exploit_debug.cpio`**: (Essential) The packaged CPIO filesystem image loaded directly into QEMU containing BusyBox, the `/init` script, and the `exploit` executable.

## Generated Databases & JSON
- **`target_db.kxdb`**: (Essential) The finalized Kernel eXploit Database holding all localized structure offsets and verified JOP/ROP gadget addresses for the target kernel build. Generated via `kxdb_tool.py`.
- **`rop_actions.json`**: (Essential) The raw output from `angrop_rop_generator.py` documenting the semantic offsets (e.g., `pop rdi; ret` found at `0x6fe9`).
- **`stack_pivots.json`**: (Essential) The raw output from `pivot_finder.py` listing the exact sequential register manipulation gadgets required for the JOP bridge.
- **`btf.json`**: (Optional / Debug) The JSON representation of the BTF Type Information extracted via `bpftool`, used natively by `extract_structures.py`.

## Runtime Logs
- **`qemu_output.log`**: (Essential) Total standard output captured from `start_qemu.sh` containing the exact AAR leak prints, the `READY_FOR_GDB` breakpoint signal, and the conclusive `Win!` shell evidence.
- **`qemu_panic.log`**: (Essential) Text logs of the QEMU console during the various development panics (e.g., hitting `0xffffffff810001bd`), providing the canonical register state at crash-time.
- **`gdb_trace.log`**: (Essential) Live instruction-level trace mapping the successful transition of the `PIVOT1` -> `PIVOT2` -> `PIVOT3` -> `PIVOT4` bridge proving the internal `libxdk` parser functioned flawlessly.
- **`gdb_trace_dump.log`**: (Essential) Post-mortem memory dumps of `virt+0x120` capturing the actual 64-bit pointers `exploit.cpp` deposited onto the heap immediately before the exploit fired.

## Instrumentation Scripts
- **`trace_qemu.py` / `trace_gdb.py`**: (Essential) Automation scripts used exclusively to hook into QEMU's GDB stub to bypass the timing-sensitive race conditions of the `epoll` UAF and silently inspect ROP assembly layout.
- **`get_kernel_rop_0.py`**: (Essential) A specific extraction script used to prove that `kernel_rop[0]` explicitly contained the invalid offset due to database desynchronization.

## Archival Documents (Markdown)
- **`01_WORKING_STATE.md` to `10_EVIDENCE_MANIFEST.md`**: (Essential) Historical context files generated previously to chronicle the exact engineering steps taken to overcome the initial blockages. They reference all the above logs for forensic accountability.
