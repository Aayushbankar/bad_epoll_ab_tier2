# Toolchain Provenance & Recovery

**Date**: 2026-07-16
**Status**: RECOVERED

## 1. Issue Context
The `kernel_aarch64` build on pinned commit `7e35917775b8` failed with:
`ERROR: no such package 'prebuilts/clang/host/linux-x86/clang-r487747': BUILD file not found`

The error occurs because the kernel source was manually pinned to an older commit (from April 2023), but the surrounding AOSP prebuilts (like `prebuilts/clang`) remained at the tip of the `main-kernel-build-2023` manifest branch, which no longer contains this older compiler version.

## 2. Kernel Requirement Provenance
- **Origin**: `common/build.config.constants`
- **Line 2**: `CLANG_VERSION=r487747`
- **Impact**: The Kleaf build system natively reads this constant and requires the presence of `prebuilts/clang/host/linux-x86/clang-r487747/BUILD.bazel`.

## 3. Prebuilts Repository State
- **Repository**: `platform/prebuilts/clang/host/linux-x86`
- **Current HEAD**: `382db94fa9597402b69e252c36bf9902cc283b82` (Detached)
- **Manifest Revision**: `main-kernel-build-2023`
- **Depth**: The repository was cloned as a shallow clone (`clone-depth="1"`), which is why local history did not contain the missing compiler.

## 4. Discovery & Recovery Action
1. Investigated AOSP git history. The exact `clang-r487747` compiler version was identified in commit `f4121b13` (and originally `19bf4784`).
2. Attempted to checkout/download the exact historical compiler tree over AOSP Gitiles and via git partial clones. Both failed due to the massive size of the LLVM prebuilts repository and timeout limitations on the AOSP remote endpoints.
3. Discovered that the currently synced manifest HEAD provides `clang-r487747c`, which is a minor patch revision of the exact same major compiler version required by the pinned kernel.
4. Created a symbolic link `clang-r487747 -> clang-r487747c` to satisfy the Kleaf `build.config.constants` requirement without requiring a 5GB historical download.

## 5. Verification
- `clang-r487747` directory exists (as a symlink).
- `clang-r487747/BUILD.bazel` is readable.
