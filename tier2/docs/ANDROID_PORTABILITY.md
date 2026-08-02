# Android Portability Tracker

> **Status**: ACTIVE — Living document mapping Linux findings to Android reality
> **Rule**: Every verified Linux finding must have an Android Effect assessment
> **Update**: After every experiment that changes Linux or Android state

---

## Portability Matrix

| # | Linux Finding | Android Effect | PoC Impact | Remaining Blocker | Experiment |
|---|---------------|----------------|------------|-------------------|------------|
| **LF-001** | Race exists (lockless fast-path bypass) | **UNKNOWN** — GDB-assisted only | **BLOCKER** | Natural schedulability unproven; cond_resched no-op in 2-CPU PREEMPT_VOLUNTARY | NAT-001, NAT-002 (corrected) |
| **LF-002** | Primitive: NULL write at offset 160 of kmalloc-192 | Works IF race works | Medium | Depends on LF-001 | NAT-001 |
| **LF-003** | `msg_msg` (144B) reclaims freed eventpoll | **UNKNOWN** — seccomp may block `msgsnd`/`msgrcv` | **BLOCKER** | Android seccomp profile | AND-001 |
| **LF-004** | Chain 0 (`percpu_counter_dec` crash) dead | N/A — dead on Linux | None | — | — |
| **LF-005** | Chain 1 (dual-watch KASLR leak) dead | N/A — dead on Linux | None | — | — |
| **LF-006** | Chain 2 (arbitrary decrement) dead | N/A — dead on Linux | None | — | — |
| **LF-007** | `struct file` UAF dead | N/A — dead on Linux | None | — | — |
| **LF-008** | `epitem` same-cache reclaim dead | N/A — dead on Linux | None | — | — |
| **LF-009** | `snd_timer_user` reclaim dead | N/A — different caches | None | — | — |
| **LF-010** | `eventpoll_epi` isolated dedicated cache | Same on Android (SLUB config identical) | Low | Verify Android SLUB config | Static audit |
| **LF-011** | `CONFIG_SLAB_FREELIST_RANDOM=n` | Same on Android GKI | Low | Verify Android GKI config | AND-002 |
| **LF-012** | `CONFIG_SLAB_FREELIST_HARDENED=n` | Same on Android GKI | Low | Verify Android GKI config | AND-002 |
| **LF-013** | `CONFIG_INIT_ON_FREE_DEFAULT_ON=n` | Same on Android GKI | Low | Verify Android GKI config | AND-002 |
| **LF-014** | `CONFIG_INIT_ON_ALLOC_DEFAULT_ON=n` | Same on Android GKI | Low | Verify Android GKI config | AND-002 |
| **LF-015** | `CONFIG_MEMCG=n` | Same on Android GKI | Low | Verify Android GKI config | AND-002 |
| **LF-016** | `CONFIG_SLUB_CPU_PARTIAL=y` | Same on Android GKI | Medium | Per-CPU interference | NAT-004 |
| **LF-017** | `CONFIG_USERFAULTFD=n` | Same on Android GKI | Low | No uffd timing primitive | — |
| **LF-018** | `CONFIG_SYSVIPC=y` | **UNKNOWN** — may be blocked by seccomp | **BLOCKER** | Android seccomp | AND-001 |

---

## Android-Specific Mitigations Assessment

| Mitigation | Linux Config | Android GKI Status | Effect on Primitive | Verdict |
|------------|--------------|-------------------|---------------------|---------|
| **KASLR** | `CONFIG_RANDOMIZE_BASE=y` | **ON** (default) | Adds entropy to slab addresses; freed slot learned dynamically | Medium — AND-002 |
| **SELinux** | `CONFIG_SECURITY_SELINUX=y` | **ENFORCING** (default) | May block `epoll`, `msgget`, `msgsnd`, `msgrcv`, netlink | **BLOCKER** — AND-003 |
| **seccomp-bpf** | Not in kernel config | **ACTIVE** (per-app profiles) | May block `msgsnd`/`msgrcv`/`msgget` syscalls | **BLOCKER** — AND-001 |
| **MTE** | `CONFIG_ARM64_MTE=y` | **ON** (Pixel 7+) | Tags memory; UAF access may cause synchronous tag fault | Medium — AND-004 |
| **KASAN HW_TAGS** | `CONFIG_KASAN_HW_TAGS=y` | **ON** (sampling) | May detect UAF at `hlist_del_rcu` write | Medium — AND-004 |
| **PAC** | `CONFIG_ARM64_PTR_AUTH=y` | **ON** (return address signing) | Irrelevant — no control-flow hijack | None |
| **BTI** | `CONFIG_ARM64_BTI=y` | **ON** (branch targets) | Irrelevant — data-only primitive | None |
| **CFI** | `CONFIG_CFI_CLANG=y` | **ON** | Irrelevant — no indirect call corruption | None |
| **Hardened Usercopy** | `CONFIG_HARDENED_USERCOPY=n` | **ON** (Android default) | May block `copy_to_user` of corrupted data | Low |
| **PAGE_TABLE_ISOLATION** | `CONFIG_PAGE_TABLE_ISOLATION=y` | **ON** | No effect on kernel-only UAF | None |

---

## Target Device Matrix

| Device | Android Version | Kernel Base | KASLR | SELinux | MTE | PAC/BTI | Notes |
|--------|----------------|-------------|-------|---------|-----|---------|-------|
| **Pixel 6/6a** | 13/14 | 5.10/5.15 | ON | Enforcing | SW | HW | GKI |
| **Pixel 7/7a** | 13/14 | 5.15/6.1 | ON | Enforcing | HW | HW | GKI, MTE HW |
| **Pixel 8** | 14 | 6.1/6.6 | ON | Enforcing | HW | HW | GKI |
| **AVD (emulator)** | 14 (API 34) | 6.1 (`7e359177`) | OFF (`nokaslr`) | Enforcing | OFF | OFF | **Current test env** |
| **Generic GKI** | 14 | 6.1/6.6 | ON | Enforcing | Config | Config | Production baseline |

**Current Test Gap**: AVD runs with `kasan=off nokaslr` — does not match production.

---

## Syscall Availability by Context

| Syscall | Shell (adb) | Untrusted App (`run-as`) | System Server | Notes |
|---------|-------------|--------------------------|---------------|-------|
| `epoll_create1` | ✅ | ❓ | ✅ | Core syscall |
| `epoll_ctl` | ✅ | ❓ | ✅ | Core syscall |
| `epoll_wait` | ✅ | ❓ | ✅ | Core syscall |
| `msgget` | ✅ | ❓ | ✅ | SysV IPC |
| `msgsnd` | ✅ | ❓ | ✅ | SysV IPC |
| `msgrcv` | ✅ | ❓ | ✅ | SysV IPC |
| `msgctl` | ✅ | ❓ | ✅ | SysV IPC |
| `add_key` / `keyctl` | ✅ | ❓ | ✅ | `CONFIG_KEYS=y` |
| `setxattr` / `fsetxattr` | ✅ | ❓ | ✅ | `CONFIG_TMPFS_XATTR=y` |
| `open("/dev/snd/timer")` | ❓ | ❌ | ❓ | SELinux `audioserver` only |
| netlink `RTM_NEWROUTE` | ✅ (CAP_NET_ADMIN) | ❌ | ✅ | Needs capability |

**Critical Gap**: ❓ = **UNKNOWN** — must test via AND-001/003

---

## Required Android Configuration Verification

| Config | Current Test Kernel | Android GKI Default | Verification Method |
|--------|---------------------|---------------------|---------------------|
| `CONFIG_RANDOMIZE_BASE` | OFF (`nokaslr`) | ON | Boot with `kaslr` |
| `CONFIG_KASAN` | OFF (`kasan=off`) | ON (sampling) | Boot with `kasan=on` |
| `CONFIG_KASAN_HW_TAGS` | OFF | ON (MTE devices) | Boot with `kasan=on` |
| `CONFIG_ARM64_MTE` | OFF | ON (Pixel 7+) | Check `/proc/cpuinfo` |
| `CONFIG_SECURITY_SELINUX` | ON | ON (Enforcing) | `getenforce` |
| `CONFIG_SYSVIPC` | ON | ON | `ls /proc/sys/kernel/msg*` |
| `CONFIG_KEYS` | ON | ON | `keyctl` test |
| `CONFIG_TMPFS_XATTR` | ON | ON | `setxattr` test |
| `CONFIG_USERFAULTFD` | OFF | OFF | — |
| `CONFIG_PREEMPT_VOLUNTARY` | ON | Likely ON | Check `.config` |
| `CONFIG_SLUB_CPU_PARTIAL` | ON | Likely ON | Check `.config` |

---

## Exploit Portability Requirements

| Requirement | Linux Status | Android Gap | Resolution |
|-------------|--------------|-------------|------------|
| Race naturally schedulable | **NO** (GDB only) | Must prove on device | NAT-001/002 |
| `msg_msg` spray works | **GDB-only** | seccomp/SELinux | AND-001/003 |
| KASLR bypass not needed (DoS) | N/A (DoS only) | DoS works if race works | NAT-001 |
| MTE doesn't prevent UAF | N/A | May cause early crash | AND-004 |
| SELinux allows exploit syscalls | N/A | Must verify per-syscall | AND-003 |
| No PAC/BTI bypass needed | ✅ (data-only) | Confirmed irrelevant | — |
| No CFI bypass needed | ✅ (data-only) | Confirmed irrelevant | — |

---

## PoC Development Roadmap (Android)

```
Phase A: Linux Validation (CURRENT)
├── NAT-001: Natural race statistical proof
├── NAT-002: Preemption point identification
├── AND-001: SysV IPC on AVD
├── AND-002: KASLR impact
├── AND-003: SELinux policy
└── AND-004: MTE/KASAN impact

Phase B: Android AVD Validation
├── Rebuild kernel: KASLR+KASAN+SELinux enforcing
├── Run NAT-001 on production config
├── Verify msg_msg spray in app context
├── Test alternative sprays (add_key, setxattr)
└── Measure hit rate with all mitigations ON

Phase C: Device Porting (Pixel 7/8)
├── Extract device kernel config
├── Build exploit binary for target arch
├── Test via adb shell (root not needed for DoS)
├── Verify crash signature in `last_kmsg`
└── Document device-specific offsets

Phase D: Production PoC
├── Reliable DoS PoC (kernel crash)
├── Optional: Info leak if Chain 1 revived
├── Optional: LPE if new primitive found
└── Stability testing (no watchdog panic)
```

---

## Current Blockers (Ranked)

| Blocker | Severity | Depends On | ETA |
|---------|----------|------------|-----|
| **Natural race unproven; cond_resched no-op** | CRITICAL | NAT-001 redesign with timing-widening | Week 1 |
| **msg_msg blocked by seccomp** | CRITICAL | AND-001 | Week 1 |
| **SELinux denies exploit syscalls** | CRITICAL | AND-003 | Week 1 |
| **KASLR reduces hit rate to 0** | HIGH | AND-002 | Week 1 |
| **MTE crashes on UAF write** | MEDIUM | AND-004 | Week 2 |
| **Per-CPU partial breaks cross-CPU spray** | MEDIUM | NAT-004 | Week 1 |

---

## Sign-Off Criteria for Android PoC

- [ ] Natural race hit rate > 0 on `PREEMPT_VOLUNTARY` kernel (10k iterations)
- [ ] Race works with `kasan=on nokaslr` → `kasan=on kaslr`
- [ ] Race works with SELinux enforcing + seccomp profile
- [ ] `msg_msg` spray works in target context (shell/app)
- [ ] MTE/KASAN_HW_TAGS doesn't prevent race detection
- [ ] Kernel crash signature: `hlist_del_rcu` / `__ep_remove` in backtrace
- [ ] Reproducible on Pixel 7/8 GKI kernel
- [ ] No watchdog panic on crash (clean crash dump)

---

**Last Updated**: 2026-08-02
**Next Update**: After AND-001, AND-002, AND-003 completion