# Assumption Register

| Assumption | Evidence | Validation Status | Date | Notes |
| :--- | :--- | :--- | :--- | :--- |
| `kernel-ranchu` exactly matches `7e35917775b8` | `extract-ikconfig` embedded version string output | Validated | 2026-07-13 | Extracted string confirms exact commit hash. |
| Rebuilding source produces exact matching symbols | Kleaf standardizes toolchains and hermetic builds | Pending Compilation | 2026-07-13 | Must verify via SHA-256 and GDB breakpoint resolution. |
| BTI is enforced on all loaded modules | Extracted `/proc/config.gz` equivalent shows `CONFIG_ARM64_BTI_KERNEL=y` | Partially Validated | 2026-07-13 | Need to confirm if exceptions are actively generated on indirect branching during exploit tests. |
| `f_op->poll` hijack is impossible | PAC is enabled (`CONFIG_ARM64_PTR_AUTH_KERNEL=y`) | Pending Exploitation Test | 2026-07-13 | Will attempt a direct overwrite and observe the resulting panic signature. |
