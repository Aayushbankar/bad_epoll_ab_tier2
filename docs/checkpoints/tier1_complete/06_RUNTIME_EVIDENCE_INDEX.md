# Runtime Evidence Index

This index catalogs every physical runtime artifact collected during the investigation and execution of the Tier 1 exploit. These files are stored in the `artifacts/` archive or tracked within the `exploit/tier1-linux-vm` directory.

## Tracing & Instrumentation Scripts
- `trace_qemu.py`: Python GDB automation script to hook the exploit execution flow right before the vulnerability triggers.
- `trace_gdb.py`: Python GDB parsing script mapping `libxdk` parser internals to physical execution addresses.
- `rop_extractor.py`: Python script used to extract and verify the actual values deposited into the ROP payload memory page.
- `get_kernel_rop_0.py`: GDB script specifically constructed to intercept and print `kernel_rop[0]` directly out of live memory to prove data provenance.
- `run_qemu_gdb.sh` / `start_qemu.sh`: Scripts configuring the QEMU target to open the standard `:1234` gdbserver port without freezing boot.
- `run_gdb_interactive.sh` / `run_gdb_dump.sh`: Invocation wrappers applying the trace plugins and logging the output cleanly to files.

## Logs & Outputs
- `qemu_output.log`: The full STDOUT output of the exploit binary from within QEMU, culminating in `Win! UID: 0, GID: 0, EUID: 0`.
- `qemu_panic.log`: Documentation of the kernel's stack trace immediately upon panic after hitting the invalid database ROP offset (`0xffffffff810001bd`).
- `gdb_trace.log`: GDB instruction-level trace mapping the successful transition across `PIVOT1` -> `PIVOT2` -> `PIVOT3` -> `PIVOT4` into the payload page.
- `gdb_trace_dump.log`: Live memory dumps of the payload page showcasing exactly what data was written by `exploit.cpp` prior to the payload hijack.
- `expect.log`: `pexpect` automation logs.

## Databases & Binaries
- `target_db.kxdb`: The finalized, regenerated KXDB format database containing the valid gadget offsets.
- `target_db.kxdb.bak`: The initial (failing) database targeting the official kernelCTF build.
- `vmlinux` / `vmlinuz`: The locally compiled vulnerable Linux 6.12.67 kernel images.
- `exploit`: The compiled `exploit.cpp` PoC statically linked against `libkernelXDK.a`.
