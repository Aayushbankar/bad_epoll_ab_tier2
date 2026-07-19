# Source Tree Provenance & Verification

**Date**: 2026-07-16
**Status**: TARGET COMMIT MISMATCH

## Repo Manifest Configuration
- **Project**: `kernel/common`
- **Configured Remote Upstream**: `android14-6.1`
- **Manifest Revision**: `8824f345abdb23c887f48c05f704cf2dcd56afc1`
- **Evidence**: Output of `repo info common` and `repo manifest -r`.

## Current Local Checkout (`common/`)
- **Current HEAD**: `8824f345abdb23c887f48c05f704cf2dcd56afc1`
- **Current Branch**: Detached HEAD ("Not currently on any branch")
- **Repository Cleanliness**: `nothing to commit, working tree clean`

## Ground-Truth Target Verification
- **Target Commit**: `7e35917775b8` (Extracted from emulator `kernel-ranchu` string `6.1.23-android14-4-00257-g7e35917775b8`)
- **Existence**: Commit `7e35917775b8b3e3346a87f294e334e258bf15e6` exists locally within the repository history.
- **Evidence**: `git show 7e35917775b8` confirms it is present in the local Git object database.

## Source-Tree Completeness
All required build scripts, subsystems, and reverse-engineering target files are verified present:
- `BUILD.bazel`, `Makefile`, `tools/bazel`
- `fs/eventpoll.c`, `kernel/cred.c`, `include/linux/sched.h`, `mm/slub.c`, `include/linux/fs.h`, `fs/pipe.c`, `ipc/msgutil.c`, `security/selinux/`, `drivers/android/binder.c`

## Conclusion & Next Action
**TARGET COMMIT MISMATCH**. The `repo sync` correctly downloaded the Android 14 GKI tree, but it checked out the tip of the manifest (`8824f345...`) rather than the exact commit corresponding to our emulator's precompiled kernel (`7e35917775b8`). 

If we compile from `8824f345...`, the resulting `vmlinux` symbols and structure offsets will deviate from the runtime kernel in the emulator, breaking exploit reproducibility.

**Safest Next Action**: Manually checkout the target commit inside the common project using `git checkout 7e35917775b8` before initiating the Bazel kernel compilation.
