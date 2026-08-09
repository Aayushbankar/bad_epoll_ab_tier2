# AND-003 SELinux Enforcing Syscall Audit

**Status**: VERIFIED

## Description
This experiment tested whether SELinux in enforcing mode (on Android 14 GKI `linux-6.12.67`) blocks any of the key syscalls required by the CVE-2026-46242 exploit chain. The syscalls audited were `epoll_create1`, `epoll_ctl`, `close`, `msgget`, `msgsnd`, and `msgrcv`.

## Findings
All required syscalls completed successfully under SELinux `enforcing=1`. The exploit primitives are not restricted by SELinux.

## Raw Evidence
Captured from `tier2/evidence/AND-003_raw_enforcing.log`:

```
[AND-003] SELinux Enforcing Syscall Audit
[AND-003] PASS: epoll_create1 success
[AND-003] PASS: epoll_ctl success
[AND-003] PASS: close success
[AND-003] PASS: msgget success
[AND-003] PASS: msgsnd success
[AND-003] PASS: msgrcv success
[AND-003] Audit Complete
```
