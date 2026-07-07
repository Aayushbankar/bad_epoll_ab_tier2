# bad-epoll-lab

> Hands-on recreation and analysis of CVE-2026-46242 ("Bad Epoll") — a race condition UAF in the Linux kernel's `epoll` subsystem that achieves local privilege escalation to root.

## Project Goal

Recreate the kernelCTF PoC locally, understand every stage of the exploit chain, test it on Android, and produce a professional write-up for Medium/LinkedIn.

## Vulnerability Summary

| Field | Value |
|-------|-------|
| **CVE** | CVE-2026-46242 |
| **Nickname** | "Bad Epoll" |
| **Type** | Race Condition → Use-After-Free (UAF) |
| **Subsystem** | `fs/eventpoll.c` |
| **Affected Kernels** | 6.4-rc1 through 7.0 (introduced by commit `58c9b016e128`) |
| **Fix** | Commit `a6dc643c693` (mainline 2026-04-24) |
| **Impact** | Local unprivileged user → root (uid=0) |
| **Reliability** | ~99% on LTS, ~98% on COS |
| **Android Impact** | Devices running kernel 6.6+ (Android 16+ flagships) |

## Exploit Chain Overview

```
KASLR Leak (prefetch side-channel)
  → Race Condition Trigger (close-vs-close on epoll fds)
    → UAF Write-0 (zeroes offset 160 of freed kmalloc-192)
      → Escalate to struct file UAF (dangling pointer)
        → Cross-Cache Attack (reclaim filp slab as pipe_buffer)
          → Constrained AAR (read kernel memory via /proc/self/fdinfo)
            → Full AAR (walk task_struct tree)
              → RIP Control + ROP (hijack f_op->poll, stack pivot)
                → Root Shell (commit_creds(&init_cred), execve)
```

## Repository Structure

```
bad-epoll-lab/
├── README.md                          # This file
├── docs/
│   ├── CONTEXT.md                     # AI agent context (read this first)
│   ├── VULNERABILITY.md               # Root cause analysis
│   ├── EXPLOIT_WALKTHROUGH.md         # Step-by-step exploit mechanics
│   ├── PROGRESS.md                    # Progress tracker
│   ├── REPRODUCIBILITY_REPORT.md      # Static analysis of repo reproducibility
│   ├── ENVIRONMENT_REBUILD_GUIDE.md   # How to rebuild the environment from scratch
│   ├── MENTOR_STATUS_REPORT.md        # High-level technical status report
│   └── ENVIRONMENT_CONSTANTS.md       # Custom kernel layout and struct offsets
├── exploit/
│   ├── tier1-linux-vm/                # Tier 1: QEMU Linux VM setup
│   ├── tier2-android-emulator/        # Tier 2: Android emulator setup
│   └── tier3-selinux-analysis/        # Tier 3: SELinux bypass research
├── scripts/
│   ├── setup-tier1.sh                 # Automated Tier 1 setup script
│   └── setup-tier2.sh                 # Automated Tier 2 setup script
├── logs/                              # dmesg, logcat, exploit output logs
└── article/
    └── draft.md                       # Medium/LinkedIn article draft
```

## Current Status & Accomplishments (As of July 2026)

**Accomplished (Tier 1):**
* Compiled Linux Kernel 6.12.67 from source and built a QEMU test environment.
* Ported the Google kernelCTF exploit to our custom environment, fixing modern C++ compilation errors in `libxdk`.
* Achieved a highly reliable Use-After-Free (UAF) trigger by dynamically recalibrating the `epoll` race condition thresholds for nested virtualization overhead.
* Bypassed KASLR and achieved Arbitrary Address Read (AAR) by reverse-engineering our custom `task_struct` offsets.

**Unresolved / Current Blocker:**
* The exploit panics at the final RIP hijack stage (`__x86_indirect_call_thunk_rdi+0x5`). 
* The selected stack pivot gadget was misinterpreted (AT&T syntax confusion), resulting in a supervisor write fault. A new, true stack pivot gadget must be found in `vmlinux` and patched into the exploit to achieve the final root shell.
* Exploit compilation relies on manual source code patches and is not yet fully automated.

## Rebuilding the Environment

If you need to rebuild the environment from scratch on an empty Linux machine, refer to our comprehensive guide:
👉 **[ENVIRONMENT_REBUILD_GUIDE.md](docs/ENVIRONMENT_REBUILD_GUIDE.md)**

For a static analysis of the repository's reproducibility state, see the **[REPRODUCIBILITY_REPORT.md](docs/REPRODUCIBILITY_REPORT.md)**.

## Three-Tier Approach

| Tier | Environment | Goal | Est. Time |
|------|-------------|------|-----------|
| 🟢 Tier 1 | Linux VM (QEMU) | Learn exploit mechanics, get root | **Paused (Blocked)** |
| 🟡 Tier 2 | Android Emulator | Prove it works on Android | Pending |
| 🔴 Tier 3 | SELinux Analysis | Real-world severity assessment | Pending |

## Prerequisites

- Linux host (Fedora/Ubuntu)
- `build-essential`, `qemu-system-x86`, `busybox-static`, `cpio`
- Android SDK (installed) + NDK (to be installed for Tier 2)
- ~20GB free disk space for kernel compilation

## References

- [Original PoC (kernelCTF submission)](https://github.com/J-jaeyoung/security-research/tree/submit-cve-2026-46242/pocs/linux/kernelctf/CVE-2026-46242_lts_cos)
- [Exploit writeup](https://github.com/J-jaeyoung/security-research/blob/submit-cve-2026-46242/pocs/linux/kernelctf/CVE-2026-46242_lts_cos/docs/exploit.md)
- [Vulnerability writeup](https://github.com/J-jaeyoung/security-research/blob/submit-cve-2026-46242/pocs/linux/kernelctf/CVE-2026-46242_lts_cos/docs/vulnerability.md)
- [Fix commit](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/commit/?id=a6dc643c69311677c574a0f17a3f4d66a5f3744b)
- [Introducing commit](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/commit/?id=58c9b016e12855286370dfb704c08498edbc857a)

## License

This repository is for educational and authorized security research purposes only. Do not use any code or techniques from this repository for unauthorized access to systems.
