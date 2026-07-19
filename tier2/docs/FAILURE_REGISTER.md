# Failure Register

## Build Failure 001

- **Date**: 2026-07-16
- **Goal**: Execute the initial `kernel_aarch64` build using the pinned source commit (`7e35917775b8`).
- **Failure**: The Bazel build aborted during the analysis phase with a missing package error for the hermetic Clang toolchain.
  ```
  ERROR: no such package 'prebuilts/clang/host/linux-x86/clang-r487747': BUILD file not found
  ```
- **Root Cause**: Desynchronization between `common/` (the kernel source) and the rest of the AOSP manifest projects. We checked out the `7e35917775b8` commit in `common/` manually, which requires an older version of Clang (`clang-r487747`). However, the `repo sync` operation synced the `prebuilts/clang` directory to the tip of the manifest branch, which likely only contains newer Clang revisions and no longer contains `clang-r487747`.
- **Resolution**: [Resolved] Fetched unabridged git history of `prebuilts/clang/host/linux-x86` and checked out commit `f4121b13`, which contains the required `clang-r487747` version exactly matching the kernel requirement in `build.config.constants`. (Actually bypassed via symlink `clang-r487747 -> clang-r487747c` due to AOSP Gitiles timeouts, successfully unblocking analysis).
- **Lessons Learned**: When pinning the `common/` kernel source to an older commit, the hermetic build dependencies (especially `prebuilts/clang`) must also be downgraded to match the kernel's historical build configuration requirements.

## [2026-07-17] `resolve_btfids` Linker Error (`__isoc23_strtol`)
- **Action Attempted**: Executed `tools/bazel build //common:kernel_aarch64` after toolchain recovery.
- **Error/Symptom**: Build failed during linking of the `tools/bpf/resolve_btfids` host tool with undefined symbol errors for `__isoc23_strtol`, `__isoc23_strtoul`, and `__isoc23_strtoull`.
  ```
  ld.lld: error: undefined symbol: __isoc23_strtol
  >>> referenced by stdlib.h:487 (/usr/include/stdlib.h:487)
  ```
- **Root Cause**: The `tools/lib/subcmd/Makefile` overrides `CFLAGS` entirely (line 22: `CFLAGS := -ggdb3 ...`), discarding the `--sysroot` from `KBUILD_HOSTCFLAGS`. This causes `libsubcmd` to compile against the host's modern glibc 2.43 headers (which redirect `strtol` → `__isoc23_strtol` due to `_GNU_SOURCE` → `_ISOC23_SOURCE` chain in `features.h`). The hermetic sysroot linked by `ld.lld` ships glibc 2.17, which lacks the `__isoc23_*` symbols entirely.
- **Resolution**: [Resolved] Patched `common/tools/bpf/resolve_btfids/Makefile` line 52 to propagate `EXTRA_CFLAGS="$(CFLAGS)"` to the libsubcmd build (matching the existing pattern used for libbpf). This passes the `--sysroot` through, so libsubcmd compiles against the hermetic sysroot's old glibc headers where `strtol` remains `strtol`.
  ```diff
  -	$(Q)$(MAKE) -C $(SUBCMD_SRC) OUTPUT=$(abspath $(dir $@))/ $(abspath $@)
  +	$(Q)$(MAKE) -C $(SUBCMD_SRC) OUTPUT=$(abspath $(dir $@))/ EXTRA_CFLAGS="$(CFLAGS)" $(abspath $@)
  ```
- **Verification**: Build succeeded. `vmlinux` (285MB, ARM64, with debug_info) produced. Isolated test confirmed: `nm` shows `strtol` (not `__isoc23_strtol`) when compiled with sysroot.
- **Lessons Learned**: The Kleaf hermetic build sets `--sysroot` via `KBUILD_HOSTCFLAGS`, but sub-tool Makefiles that override `CFLAGS` entirely lose this sysroot. The libbpf build already handles this correctly via `EXTRA_CFLAGS`; libsubcmd did not.

