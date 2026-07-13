# Failure History

## 1. Supervisor Write Fault on Exploit Pivot
- **Symptom:** The kernel threw a Supervisor Write Fault exactly after `__x86_indirect_call_thunk_rdi` was invoked to execute the JOP stack pivot.
- **Root Cause:** A pre-compiled `target_db.kxdb` shipped with the repository contained ROP gadget offsets specific to the official kernelCTF `vmlinux` binary. However, the repository environment compiled a local `vmlinux` using Fedora GCC 16.1.1. This compiler discrepancy shifted the `.text` segment layout, resulting in the exploit vectoring into invalid instruction offsets.
- **How it was discovered:** Initial analysis incorrectly blamed AT&T vs Intel syntax interpretation of `PIVOT4`. However, a rigorous runtime GDB trace of the JOP chain proved the pivots executed flawlessly, pushing the invalid address `0xffffffff810001bd` directly into `RIP`.
- **How it was fixed:** Installed the complete dependency stack (`angrop`, `rp++`, `bpftool`, etc.), generated a new profile dataset (`rop_actions.json`, `stack_pivots.json`) against the local `vmlinux`, and used `kxdb_tool` to regenerate the `target_db.kxdb` file with accurate offsets.

## 2. Angrop Pickling Error During Generation
- **Symptom:** `angrop_rop_generator.py` crashed midway through execution with a `_pickle.PicklingError`.
- **Root Cause:** Python 3.14's `multiprocessing` library could not serialize the `SpecialMem` class because it was nested dynamically inside the `make_initial_state` function within `angrop/rop_utils.py`.
- **How it was discovered:** Standard error output from the pipeline script provided the Python traceback.
- **How it was fixed:** Modified the `angrop` library source directly, lifting `class SpecialMem` into the global module scope so the `forkserver` could successfully serialize it.

## 3. Exploit Silent Crash in Userspace
- **Symptom:** The exploit outputted `Win!` successfully but then the kernel immediately crashed with `Attempted to kill init!`.
- **Root Cause:** The exploit successfully returned to userspace and executed `execve("/bin/bash")`, but `/bin/bash` did not exist in our static BusyBox `initramfs`. The exploit fell back to `_exit(0)`, which terminated PID 1, thereby instantly halting the kernel. Even after replacing it with `/bin/sh`, it silently failed because QEMU was being run in the background without a controlling TTY, causing `/bin/sh` to hit EOF and exit.
- **How it was discovered:** Added `perror("execve");` and modified the invocation to non-interactively dump evidence using `/bin/sh -c "id; uname -a; echo ROOT_SHELL_SUCCESS"`.
- **How it was fixed:** We replaced the interactive shell call with a non-interactive `-c` execution flag for validation purposes, allowing it to correctly print the root execution success parameters before cleanly crashing.
