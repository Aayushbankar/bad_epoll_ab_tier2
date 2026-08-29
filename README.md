# CVE-2026-46242: Bad Epoll (Tier 2 Android ARM64 Assessment)

[![Status](https://img.shields.io/badge/Verdict-DoS--Only%20(AMBER)-orange.svg)](#verdict)
[![Target](https://img.shields.io/badge/Target-Android%2014%20GKI%206.1.23%20ARM64-blue.svg)](#environment)
[![Evidence](https://img.shields.io/badge/Evidence-42%20VER%20%7C%2021%20Dead%20Ends-green.svg)](tier2/docs/VERIFICATION_LEDGER.md)

Welcome to the canonical public research repository for **CVE-2026-46242** ("Bad Epoll"), a race-driven Use-After-Free (UAF) vulnerability in the Linux kernel's event polling (`epoll`) subsystem. This repository documents the complete **Tier 2** engineering assessment targeting the **Android 14 Generic Kernel Image (GKI) on ARM64**, establishing why modern kernel mitigations (PAC, BTI, kCFI, MTE, slab isolation, and voluntary preemption models) structurally reduce this critical privilege escalation primitive to **Denial of Service (DoS) only**.

> ### 📌 Research Scope & Attribution
> - **Original Vulnerability & Exploit**: Discovered, analyzed, and exploited by security researcher **Jaeyoung Chung** ([github.com/J-jaeyoung/bad-epoll](https://github.com/J-jaeyoung/bad-epoll)), submitted to Google's **kernelCTF** program (CVSS 7.8, ~99% reliable root exploit on x86_64 target).
> - **Project Scope & Contribution**:
>   - **Tier 1 (x86_64 Linux VM)**: Independent reproduction, toolchain porting (Fedora GCC 16, gadget database regeneration), and runtime verification of the kernelCTF primitive.
>   - **Tier 2 (ARM64 Android GKI)**: Original portability and exploitability research evaluating whether the primitive survives modern Android kernel mitigations (PAC, BTI, kCFI, MTE, slab isolation) — establishing a documented negative result (DoS-only).

---

## 🧭 Start Here

If you are exploring this research for the first time:

1. 📖 **[Public Deep-Dive Article](article/MEDIUM_DEEP_DIVE_FINAL.md)** — Comprehensive technical walkthrough with architecture diagrams, slab layouts, and the 21-dead-end decision tree.
2. 📋 **[Mentor Progress Writeup](docs/MENTOR_PROGRESS_WRITEUP_2026-08-29.md)** — Internal audit writeup detailing objectives, methodology, proven/disproven primitives, blockers, and next steps.
3. 📊 **[Tier 2 Complete Exploitability Report](tier2/docs/TIER2_COMPLETE_REPORT.md)** — Full engineering report containing the decision matrix, metrics dashboard, and runtime findings.
4. 🔬 **[Physical Silicon vs Emulation Analysis](docs/29_PHYSICAL_ARM64_SILICON_REBOOT_ANALYSIS.md)** — First-principles evaluation of QEMU TCG vs bare-metal ARM64 hardware timing.
5. 📂 **[Master Documentation Index](docs/MASTER_INDEX.md)** — Index of all technical reports, analyses, and daily logs.

> 🔗 **Looking for the working x86_64 root exploit?**  
> See the companion **[Tier 1 Repository (`bad-epoll-lab`)](https://github.com/Aayushbankar/bad-epoll-lab)** for the full x86_64 exploit chain (`pipe_buffer` cross-cache, AAR, KASLR bypass, JOP/ROP, UID 0).

---

## 🎯 Verdict & Key Findings

| Dimension | Tier 1 (x86_64 Linux) | Tier 2 (ARM64 Android GKI) |
|---|---|---|
| **Vulnerability Status** | Verified & Reproducible (~99% hit rate) | Verified with GDB Assist (0/102,740 Natural Hits) |
| **Exploitation Primitive** | Arbitrary Address Read + RIP Control | Fixed 8-byte NULL write @ offset 160 |
| **Outcome** | **Privilege Escalation (UID: 0)** | **Denial of Service Only (Kernel Panic)** |
| **Mitigations Overcome** | SMEP, SMAP, KPTI, KASLR | None (PAC, BTI, kCFI, MTE standing) |
| **Repository Status** | [bad-epoll-lab (Tier 1)](https://github.com/Aayushbankar/bad-epoll-lab) | Canonical Tier 2 Research Base |

### Key Technical Conclusions:
1. **The UAF is Real**: Thread A (`__ep_remove`) and Thread B (`eventpoll_release`) race to free `struct eventpoll` (176 bytes) into `kmalloc-192` (`VER-026`).
2. **Reclaim is Deterministic**: `msg_msg` allocations (144-byte user data) reliably reclaim the freed slot (`VER-027`).
3. **The Primitive is Capped**: The sole UAF write is `hlist_del_rcu(&epi->fllink)`, writing an 8-byte NULL at offset 160 (`VER-032`). On GKI 6.1, offset 160 lands inside `msg_msg` payload or corrupts kernel pointers into immediate panics.
4. **All 4 Exploitation Chains Collapsed**: 21 distinct dead ends documented across controlled crash, dual-watch KASLR leak, arbitrary decrement, and LPE paths (`DEAD_ENDS_REGISTER.md`).
5. **Scheduler Isolation**: In `PREEMPT_VOLUNTARY`, `__ep_remove` has zero preemption points, bounding the natural race window to ~250–550 cycles (~125–275 ns).

---

## 🔬 Evidence-First Methodology

This repository follows a strict verification protocol to prevent confirmation bias:

- **[Verification Ledger](tier2/docs/VERIFICATION_LEDGER.md)**: 42 machine-parseable claims mapped directly to hardware watchpoint traces and disassembly audits.
- **[Dead Ends Register](tier2/docs/DEAD_ENDS_REGISTER.md)**: 21 closed paths with explicit killing evidence and experiment citations.
- **[Assumptions Register](tier2/docs/ASSUMPTIONS_REGISTER.md)**: 42 hypotheses tracked across lifecycle states (`UNTESTED` → `VALIDATED` / `FALSIFIED`).
- **[Experiment Index](tier2/docs/EXPERIMENT_INDEX.md)**: Complete registry of all 19 experimental runs (`EXP-006` through `EXP-024`, `NAT-001`–`NAT-005`, `AND-001`–`AND-003`).

---

## 📁 Repository Structure

```text
.
├── README.md                 # Project front door, findings summary, navigation
├── article/                  # Publication deliverables
│   ├── MEDIUM_DEEP_DIVE_FINAL.md    # Ready-to-publish Medium technical article
│   ├── LINKEDIN_POST_FINAL.md       # Accompanying LinkedIn post
│   ├── DISTRIBUTION_PLAN.md         # Ranked cross-posting & researcher engagement strategy
│   └── archive/                     # Historical article drafts
├── docs/                     # Core documentation & milestone reports
│   ├── MASTER_INDEX.md              # Master technical index
│   ├── MENTOR_PROGRESS_WRITEUP_2026-08-29.md  # Formal mentor review writeup
│   ├── 29_PHYSICAL_ARM64_SILICON_REBOOT_ANALYSIS.md
│   └── daily/                       # Research daily logs
├── tier2/
│   ├── docs/                 # Authoritative Tier 2 research documents
│   │   ├── TIER2_COMPLETE_REPORT.md # Master exploitability assessment
│   │   ├── VERIFICATION_LEDGER.md   # SSOT claim verification matrix
│   │   ├── DEAD_ENDS_REGISTER.md    # Comprehensive dead ends register
│   │   ├── ASSUMPTIONS_REGISTER.md  # Hypotheses & lifecycle tracking
│   │   └── EXPERIMENT_INDEX.md      # Experiment execution index
│   ├── evidence/             # Curated, referenced execution & GDB trace logs
│   ├── scripts/              # Reproducers, GDB harnesses, and test automation
│   └── archive/              # Superseded exploratory logs and legacy scripts
│       └── debug_logs/       # Intermediate execution runs & test logs
└── third_party/              # Upstream dependencies & toolchain assets
```

---

## 🛠️ Reproduction & Quick Start

To reproduce the Tier 2 GDB-assisted verification harness:

1. **Launch Android GKI ARM64 Kernel in QEMU:**
   ```bash
   DEBUG=1 ./tier2/scripts/run_qemu.sh
   ```
2. **Attach GDB and Run the Deterministic Race Verification:**
   ```bash
   gdb-multiarch -batch -q -x tier2/scripts/exp_and003_gdb.py tier2/android/artifacts/vmlinux
   ```
3. Consult the **[Manual Demonstration Runbook](tier2/docs/TIER2_MANUAL_RUNBOOK.md)** for detailed step-by-step reproduction instructions.

---

## 🛡️ Responsible Disclosure & CVE Information

- **CVE ID**: CVE-2026-46242 ("Bad Epoll", CVSS 7.8)
- **Component**: Linux Kernel eventpoll subsystem (`fs/eventpoll.c`)
- **Original Discoverer & Exploit Author**: Jaeyoung Chung ([github.com/J-jaeyoung/bad-epoll](https://github.com/J-jaeyoung/bad-epoll)) via Google kernelCTF
- **Introduced**: Linux v6.4-rc1 (commit `58c9b016e128`)
- **Patched Upstream**: Linux v6.11 / v7.1-rc1 (commit `a6dc643c693`)

*This repository contains independent defensive and offensive security research analyzing mitigation efficacy on Android ARM64 Generic Kernel Images. No unpatched vulnerabilities or 0-day primitives are disclosed.*

---

## 👤 Author & Research Context

- **Author**: Aayush Bankar, Cybersecurity Analyst, CypherMatrix
- **Tooling**: Research augmented with agentic AI tooling (Antigravity) for code navigation, hypothesis generation, and evidence organization, with all execution traces and engineering decisions verified manually.

