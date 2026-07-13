# AI Agent Context Document

> **Purpose:** This document provides all necessary context for AI coding agents (Gemini, Claude, Copilot, etc.) to help complete tasks in this repository. Read this file FIRST before doing any work.

## Project Identity

- **Repository:** `bad-epoll-lab`
- **Location:** `/mnt/work/company/cyphermatrix/repos/bad-epoll-lab`
- **Owner:** Ayush (intern at CypherMatrix)
- **Supervisor:** Rathod Ruturaj Prafulsin
- **Related Knowledge Base:** `/mnt/work/company/cyphermatrix/knowledge/` (Obsidian vault)

## What This Project Is

A hands-on security research lab to:
1. **Recreate** the CVE-2026-46242 ("Bad Epoll") kernel exploit locally
2. **Understand** every stage of the exploit chain through practice
3. **Test on Android** to prove real-world severity
4. **Write** a professional article for Medium/LinkedIn

## Critical Technical Facts

### The Vulnerability
- **CVE:** CVE-2026-46242
- **Type:** Race condition in `fs/eventpoll.c` → Use-After-Free
- **Root Cause:** When `__ep_remove()` runs `WRITE_ONCE(file->f_ep, NULL)`, a concurrent `__fput()` sees `f_ep == NULL` on the lockless fast path of `eventpoll_release()`, skips cleanup, and frees the eventpoll + file objects while `__ep_remove()` still references them.
- **Introduced by:** Commit `58c9b016e128` ("epoll: use refcount to reduce ep_mutex contention")
- **Fixed by:** Commit `a6dc643c693` (mainline 2026-04-24)
- **Affected Kernels:** 6.4-rc1 through 7.0
- **NOT affected:** Kernels before 6.4 (this includes Android 14 on 5.15, Android 15 on 6.1)

### The Exploit (from kernelCTF submission by J-jaeyoung)
The exploit is a single C++ file (`exploit.cpp`) that performs these stages:
1. **KASLR Leak** — Prefetch side-channel to find kernel base address
2. **Race Trigger** — close-vs-close race on two epoll fds, widened by false sharing + timerfd interrupt
3. **UAF Write-0** — Zeroes `refs.first` (offset 160) of a freed kmalloc-192 eventpoll
4. **struct file UAF** — Creates dangling pointer to freed file via hlist corruption
5. **Cross-Cache** — Reclaims freed filp slab as pipe_buffer page for full control
6. **Constrained AAR** — Reads kernel memory through `/proc/self/fdinfo` by forging `f_inode`
7. **Full AAR** — Walks `task_struct` tree from `init_task` to find exploit's own process
8. **RIP Control** — Hijacks `file->f_op->poll` indirect call, pivots stack onto ROP chain
9. **Root** — `commit_creds(&init_cred)` + `switch_task_namespaces()` → `execve(/bin/sh)`

### Source Repository
- **URL:** `https://github.com/J-jaeyoung/security-research`
- **Branch:** `submit-cve-2026-46242`
- **Path:** `pocs/linux/kernelctf/CVE-2026-46242_lts_cos/`
- **Key files:**
  - `exploit/lts-6.12.67/exploit.cpp` — The exploit source
  - `docs/exploit.md` — Detailed exploit writeup
  - `docs/vulnerability.md` — Root cause analysis

## Three-Tier Approach

### 🟢 Tier 1: Linux VM via QEMU (CURRENT PRIORITY)
- **Target kernel:** Linux 6.12.67 LTS
- **Goal:** Compile exploit, boot vulnerable kernel in QEMU, achieve uid=0
- **Working directory:** `exploit/tier1-linux-vm/`
- **Status:** Not started

### 🟡 Tier 2: Android Emulator
- **Target:** Android emulator with custom GKI kernel 6.6+
- **Goal:** Cross-compile with NDK, push via adb, achieve uid=0 on Android
- **Working directory:** `exploit/tier2-android-emulator/`
- **Status:** Blocked on Tier 1 completion

### 🔴 Tier 3: Full Android Root + SELinux
- **Goal:** Analyze what uid=0 means under SELinux confinement
- **Working directory:** `exploit/tier3-selinux-analysis/`
- **Status:** Blocked on Tier 2 completion

## Environment Details

- **Host OS:** Fedora Linux (x86_64)
- **Available tools:** Android SDK (installed), GCC, Git
- **Missing tools:** Android NDK (needed for Tier 2), QEMU (may need install), busybox-static
- **Disk space needed:** ~20GB for kernel compilation

## Coding Standards for This Repo

- **Documentation:** All progress must be logged in `docs/PROGRESS.md`
- **Scripts:** Automation scripts go in `scripts/`, must be executable and well-commented
- **Logs:** All terminal output from exploit runs saved in `logs/` with timestamps
- **Article:** Draft written in Markdown at `article/draft.md`

## Related Files in Obsidian Knowledge Base

These files provide background context from prior research:
- `knowledge/04_security/CVE-2026-46242_Bad_Epoll.md` — Deep-dive report on the vulnerability
- `knowledge/04_security/unified_android_exploit_architecture.md` — Unified threat model
- `knowledge/04_security/android_security_research.md` — Original security research (Above the Waterline)
- `knowledge/04_security/exploit_chaining_research.md` — Exploit chain feasibility study
- `knowledge/02_project/tasks/cve_2026_46242_poc_and_writeup.md` — Task tracking
- `knowledge/08_daily/2026-07-06.md` — Daily log with the plan

## How to Help

When asked to work on this project:
1. **Read this file first** for full context
2. **Check `docs/PROGRESS.md`** to see what's been done
3. **Follow the tier system** — complete Tier 1 before moving to Tier 2
4. **Log everything** — update PROGRESS.md after completing any step
5. **Don't skip steps** — the user wants to learn, not just get a result
