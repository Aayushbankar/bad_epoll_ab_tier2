# SELinux Research Notebook

## Purpose
Examine the SELinux implementation and MAC (Mandatory Access Control) policies in Android 14 to understand restrictions on post-exploitation behavior.

## Important Source Files
- `security/selinux/hooks.c`
- `security/selinux/avc.c`

## Important Structures
- `struct task_security_struct`
- `struct cred` (security pointer)

## Important Functions
- `selinux_cred_alloc_blank()`
- `selinux_cred_transfer()`

## Execution Flow
*(To be populated after source sync and GDB tracing)*

## Memory Layout
- `security` pointer within `struct cred`.

## Exploitation Relevance
Gaining `uid=0` is insufficient on modern Android. Processes are confined by SELinux domains (e.g., `untrusted_app`). To achieve full compromise, the exploit must either rewrite the process's SID to `init` (`u:r:init:s0`), disable SELinux globally (often mitigated by KPP/Samsung Knox), or bypass the AVC (Access Vector Cache).

## Open Questions
- How is the `security` pointer initialized in `struct cred` in Android 14?
- Can we safely overwrite the SID without triggering SELinux state checks?

## Notes
- None yet.

## References
- None yet.
