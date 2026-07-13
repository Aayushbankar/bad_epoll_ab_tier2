# Evidence Checklist

This checklist guarantees all physical components of the exploit reproducibility lifecycle have been accounted for and archived into the local `artifacts/` database.

## Binary Executables & Images
- [x] `exploit` (Statically compiled payload)
- [x] `vmlinux` (Compiled kernel debug image)
- [x] `bzImage` (Compressed kernel boot image)
- [x] `initramfs_exploit_debug.cpio` (Packaged RootFS payload)

## Database Objects
- [x] `target_db.kxdb` (Regenerated for Fedora GCC)
- [x] `target_db.kxdb.bak` (Original kernelCTF image)

## Pipeline Output Data
- [x] `rop_actions.json` (angrop ROP offsets)
- [x] `stack_pivots.json` (pivot_finder JOP bridges)
- [x] `structs.json` (Python structure extraction)
- [x] `symbols.txt` (nm symbols dump)
- [x] `btf.json` (bpftool BTF dump)
- [x] `rp++.txt` (rp++ gadget lists)
- [x] `.config` (Locally used kernel config)

## Runtime Logs
- [x] `qemu_output.log` (Complete stdout from successful run)
- [x] `qemu_panic.log` (Kernel panic at 0x1bd)
- [x] `gdb_trace.log` (Instruction-level JOP bridge trace)
- [x] `gdb_trace_dump.log` (Payload page memory forensics)
- [x] `expect.log` (Automated script execution wrapper logs)

## Success Validators
- [x] Root execution `Win!` statement verified (`UID: 0`)
- [x] Output of `id` logged (`uid=0(root)`)
- [x] Output of `uname -a` logged
- [x] Final `ROOT_SHELL_SUCCESS` print captured

## System State
- [x] `SHA256SUMS` generated for all compiled/archived artifacts
- [x] Git Commit Hash captured (`414ff591eaf0ea2c77905dfea860aedf1f8ff06f`)
- [x] Compiler and OS build environment documented

*(No items are missing. The Tier 1 checkpoint is fully validated.)*
