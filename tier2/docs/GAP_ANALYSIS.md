# Exploit Porting Gap Analysis (Tier 1 vs Android GKI)

This table explicitly maps the Tier 1 Linux exploit pipeline against the constraints of the Android GKI environment.

| Exploitation Stage | Status | Evidence | Confidence Level | Relevant Source Files | Required Future Investigation |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **UAF Trigger (Race Condition)** | Unknown | Pure timing logic. Preemption models differ between Fedora (`CONFIG_PREEMPT_VOLUNTARY`) and Android GKI (`CONFIG_PREEMPT`). | Medium | `fs/eventpoll.c`, `kernel/sched/core.c` | Recompile Tier 1 trigger natively and measure success rate. |
| **Heap Spray (`msg_msg` / `pipe_buffer`)** | Modified | Android uses Hardened SLUB (`CONFIG_SLAB_FREELIST_RANDOM`). Adjacent allocations are non-deterministic. | High | `mm/slub.c`, `ipc/msgutil.c` | Determine if `msg_msg` allocations can reliably overlap with the `eventpoll_epi` cache. |
| **KASLR Leak** | Unknown | Depends on the ability to read back overlapping structures. | Medium | N/A | Find a reliable Out-Of-Bounds read or UAF read primitive. |
| **Control Flow Hijack (`f_op->poll`)** | Impossible | PAC (`CONFIG_ARM64_PTR_AUTH_KERNEL=y`), BTI (`CONFIG_ARM64_BTI_KERNEL=y`), and CFI (`CONFIG_CFI_CLANG=y`) strictly protect function pointers and forward-edge calls. | High | `/proc/config.gz` | **Crucial:** Must abandon control-flow hijacking entirely. Pivot to data-only attacks. |
| **ROP Chain Execution** | Impossible | Pointers in stack/registers cannot be arbitrarily forged due to PAC. | High | `arch/arm64/kernel/pointer_auth.c` | None. Abandon ROP. |
| **`commit_creds(prepare_kernel_cred(0))`** | Impossible | Cannot call functions directly. | High | `kernel/cred.c` | Must discover how to overwrite the `cred` struct linearly in memory via arbitrary write. |
| **Root Shell Verification** | Unknown | SELinux heavily restricts `uid=0` behavior (e.g., executing `/system/bin/sh` from an untrusted app domain). | High | `security/selinux/hooks.c` | Investigate SELinux context transition or disablement strategies (if possible without panic). |
