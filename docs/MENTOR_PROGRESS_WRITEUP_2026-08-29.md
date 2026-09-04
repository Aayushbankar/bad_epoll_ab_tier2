# Mentor Progress Report — CVE-2026-46242 (Bad Epoll)
**Date:** 2026-08-29
**To:** CypherMatrix Engineering Review
**From:** Aayush Bankar (Cybersecurity Analyst, CypherMatrix)
**Project:** CVE-2026-46242 Exploitation Research (Tier 1 + Tier 2)


## 1. Objective & Research Scope
Evaluate the exploitability and mitigation boundaries for CVE-2026-46242 ("Bad Epoll", CVSS 7.8):
- **Background**: Originally discovered and exploited by Jaeyoung Chung via Google kernelCTF (`github.com/J-jaeyoung/bad-epoll`), achieving ~99% reliability on x86_64; patched upstream in Linux kernel commit `a6dc643c693`.
- **Tier 1 (x86_64 Linux 6.12.67 / QEMU)**: Independently reproduce, adapt to local GCC 16 toolchains, and verify the original kernelCTF exploit chain to root (`UID: 0`).
- **Tier 2 (ARM64 Android 14 GKI 6.1.23 / QEMU aarch64)**: Conduct original exploitability research to determine if the primitive survives modern Android kernel mitigations (PAC, BTI, kCFI, MTE, and slab isolation).

## 2. Methodology
- Evidence-first protocol: 42 verification entries in VERIFICATION_LEDGER.md, each mapped to raw GDB traces or source audits
- 3 retracted claims (VER-010, VER-029, VER-030) — kept visible with retraction reasons
- 21 dead ends formally documented in DEAD_ENDS_REGISTER.md with killing evidence
- 42 assumptions tracked in ASSUMPTIONS_REGISTER.md with status (validated/falsified/untested)
- Adversarial self-review (EVO-009, 2026-08-02): discovered all prior race evidence was GDB-assisted, triggering Phase 3 natural schedulability testing
- Reference files: `tier2/docs/VERIFICATION_LEDGER.md`, `tier2/docs/DEAD_ENDS_REGISTER.md`, `tier2/docs/EXPERIMENT_INDEX.md`

## 3. What Worked

### Tier 1 (x86_64) — SUCCESSFUL
- UAF trigger: ~99% reliable with tuned timing constants (evidence: `.terminal_logs.txt`)
- Cross-cache reclaim: pipe_buffer → struct file (evidence: `qemu_output.log` 'cross-cache ok')
- AAR via /proc/self/fdinfo seq_file interface (evidence: GDB offset mapping in `ENVIRONMENT_CONSTANTS.md`)
- KASLR bypass: kernel_base leaked dynamically (evidence: `qemu_output.log` 'kernel_base=ffffffff81000000')
- RIP control via f_op->poll hijack (evidence: GDB stepi trace)
- Privilege escalation: commit_creds(init_cred) ROP chain → UID 0, GID 0, EUID 0 (evidence: `qemu_output.log` 'Win! UID: 0')
- Key breakthrough: offset desynchronization diagnosis — upstream target_db.kxdb was built for Google COS kernel; regenerated with local vmlinux via rp++ and angrop pipeline (required patching angrop PicklingError)
- Evidence: `artifacts/tier1_terminal_session.log`, `artifacts/qemu_output.log`, SHA256 hashes in FINAL_REPORT

Remaining issue: userland return instability — RSP misalignment on iretq causes SIGSEGV in execve wrapper (PID 1 crash). This is a post-exploitation cleanup issue, not a primitive failure.

### Tier 2 (ARM64 Android) — Partial Success
- Built Android 14 GKI kernel from source (commit 7e35917775b8) — vmlinux 285MB with debug_info
- UAF race mechanically reproduced with GDB assistance: 100% on demand (VER-026)
- Correctly identified freed object: struct eventpoll (176B) in kmalloc-192 (VER-020, after correcting from initial kmalloc-128 / eventpoll_epi misconception — EVO-005/006)
- msg_msg spray (144B user data) reliably reclaims freed slot (VER-027)
- Complete primitive characterization: only write is hlist_del_rcu at offset 160 = 8-byte NULL write (VER-028, VER-031, VER-032)
- SysV IPC confirmed functional on target kernel (VER-038)
- SELinux enforcing allows all exploit syscalls (VER-040, AND-003)
- KASLR has zero effect on race scheduling (VER-042, AND-002)

## 4. What Didn't Work

### All 4 Exploitation Chains — DEAD
- Chain 0 (percpu_counter_dec controlled crash): percpu_counter_dec operates on OUTER epoll's user, not freed INNER — VER-028 (EXP-019)
- Chain 1 (dual-watch KASLR leak): single-epitem UAF and multi-epitem pointer write are mutually exclusive (eventpoll.c:826 check) — VER-033 (EXP-024)
- Chain 2 (arbitrary decrement via fake user_struct): ep parameter is OUTER epoll; fbc = root_user+8 — VER-031 (EXP-023b)
- Chain 3 (full LPE): depends on Chains 1+2 — both dead

### Natural Race Schedulability — FAILED
- NAT-001: 0/10,000 natural hits (CI upper bound 0.0384%) — VER-034
- NAT-005: 0/92,740 hits with extreme optimization (isolcpus=1, 4MB cache eviction, 10-cycle steps). Best alignment error: 1 cycle (~16 ns) at delay=2360 — VER-039
- Root cause: cond_resched at eventpoll.c:888/903 is a NO-OP in PREEMPT_VOLUNTARY; __ep_remove contains ZERO preemption points; race window ~250-550 cycles (~125-275 ns) is below scheduling granularity — VER-035
- CRITICAL CAVEAT: TCG emulation does not model real cache-coherency/memory-bus timing. Chapter 29 analysis (2026-08-17) argues race IS physically viable on real ARM64 silicon due to asymmetric core drift, store buffer delays, and weakly-ordered memory model.

### Other Dead Ends (21 total)
- struct file UAF via type confusion: ep->mtx barrier prevents concurrent access (VER-025)
- pipe_buffer cross-cache: filp cache has SLAB_TYPESAFE_BY_RCU + size mismatch (VER-018)
- epitem same-cache reclaim: list_del_init corruption happens BEFORE free; INIT_LIST_HEAD overwrites on reclaim (VER-016)
- snd_timer_user: different slab cache (EVO-005)
- eventpoll_epi merge with kmalloc-128: SLAB_ACCOUNT prevents merge (VER-014)
- rb_erase_cached arbitrary write: operates on OUTER epoll's rbr (VER-032)
- Full list in DEAD_ENDS_REGISTER.md

## 5. Current Blockers
1. **No physical ARM64 hardware** — all Tier 2 experiments are on QEMU TCG. Need unlocked ARM64 hardware device or ARM64 KVM host.
2. **Primitive structurally capped at DoS** — even if race fires naturally on real silicon, the NULL@160 write yields only kernel panic or benign behavior on all audited kmalloc-192 structs (EXP-016).
3. **MTE untested** — AND-004 (MTE/KASAN_HW_TAGS behavior) still PLANNED. May cap impact at detected-fault DoS regardless.
4. **Tier 1 shell instability** — PID 1 segfault on return. Needs process injection or namespace escape.

## 6. Next Steps

### Recommended Path (from TIER2_COMPLETE_REPORT):
- **Path M (primary):** Pivot to deterministic vendor-driver primitives (GPU driver page-level UAFs) — deterministic, app-to-root, bypassing MTE/PAC/BTI/kCFI via direct physical memory mapping.
- **Path C (Bad Epoll closure):** Publish DoS-only negative-result writeup as case study — high research value, demonstrates mitigation efficacy.
- **Path A (optional, only if mentor requires):** 2-week physical ARM64 hardware timebox. Kill criteria: 0/~1M unaided hits → confirm DoS-only.

### Immediate Actions:
1. Finalize public writeup (Medium article) covering Tier 1 success + Tier 2 dead ends
2. Decision needed from mentor: approve Path M device acquisition or Path A timebox
3. Update overview docs to reflect Tier 1 completion (UID 0 verified)

## 7. Metrics Summary

| Metric | Value |
|--------|-------|
| Tier 1 exploit status | UID 0 achieved (shell return unstable) |
| Tier 2 exploit status | DoS-only — structurally dead for LPE |
| Experiments executed | 19 (EXP-006..024, NAT-001/002/005, AND-001/002/003) |
| GDB-assisted UAF hits | ~100% on demand |
| Natural race hits | 0 / 102,740 |
| Best timing alignment | 1 cycle (~16 ns) |
| Dead ends documented | 21 |
| Verification entries | 42 (VER-009..VER-043) |
| Retracted claims | 3 (VER-010, VER-029, VER-030) |
| Falsified assumptions | 10 (A-009..A-014, A-032, A-039..A-042) |

## 8. Evidence Index

All evidence is auditable via:
- Verification Ledger: `tier2/docs/VERIFICATION_LEDGER.md`
- Dead Ends Register: `tier2/docs/DEAD_ENDS_REGISTER.md`
- Experiment Index: `tier2/docs/EXPERIMENT_INDEX.md`
- Assumptions Register: `tier2/docs/ASSUMPTIONS_REGISTER.md`
- Raw GDB logs: `tier2/evidence/EXP-*_raw_gdb.log`, `tier2/evidence/NAT-*_raw_serial.log`
- Tier 1 evidence: `artifacts/qemu_output.log`, `artifacts/tier1_terminal_session.log`
- Tier 2 Complete Report: `tier2/docs/TIER2_COMPLETE_REPORT.md`
- Mentor Exec Summary: `tier2/docs/TIER2_MENTOR_EXEC_SUMMARY.pdf`
- Mentor Deck: `tier2/docs/TIER2_MENTOR_DECK.pptx`
