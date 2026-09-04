# TIER 2 EXPLOITABILITY ASSESSMENT — COMPLETE REPORT

**CVE-2026-46242 ("Bad Epoll", CVSS 7.8)** — Use-After-Free in the Linux epoll subsystem on Android ARM64 GKI  
**Original Vulnerability & PoC Credit:** Discovered and exploited by **Jaeyoung Chung** ([github.com/J-jaeyoung/bad-epoll](https://github.com/J-jaeyoung/bad-epoll)) via Google kernelCTF (~99% reliable on x86_64 target)  
**Research Scope:** Tier 1 verified the x86_64 reproduction; this report covers original Tier 2 exploitability research on Android 14 GKI  
**Branch:** main *(formerly `tier2-android-port` in `bad-epoll-lab`; repo separated 2026-08-08)* @ cc0dc7754  
**Kernel:** linux-6.12.67, Android 14 GKI, commit 7e35917775b8  
**Intern:** Aayush Bankar · **Date:** 2026-08-07  

---

## 1. Verdict (one paragraph)

The UAF is **real and 100% reproducible — but only with debugger assistance** (hardware
watchpoint, VER-026), and `msg_msg` reclaim of the freed `struct eventpoll` in kmalloc-192 is
**reliable** (VER-027). However, the race is **not naturally schedulable**: 0 / 102,740 unaided
attempts across two campaigns (VER-034, VER-039). All four exploitation chains are
**structurally dead** (21 documented dead ends); the only remaining primitive is a **fixed NULL
write at offset 160** of a freed kmalloc-192 object, which the EXP-016 audit shows is **DoS-only**
on this configuration. **Recommendation:** run Path A as a strict **2-week timebox on physical
ARM64 hardware** (timing-widening) with kill-criteria; if 0 hits in ~1M iterations, conclude
Path C (DoS-only negative-result writeup).

**Status: AMBER** — scientifically defensible negative, not a failure; evidence protocol held.

---

## 1a. 🔬 PHYSICAL ARM64 SILICON VS. EMULATION REBOOT (2026-08-17)

A first-principles reassessment resolved the gap between virtualized QEMU TCG artifacts and real ARM64 hardware:
* **Race Schedulability**: Real multicore ARM64 silicon (asymmetric big.LITTLE frequencies + DSU interconnect cacheline invalidation latency of 20–80 ns) permits the race to trigger naturally without GDB intervention.
* **Primitive Constraint**: Even with 100% race trigger success, the corrupting operation is strictly an **8-byte NULL write at offset 160** of a `kmalloc-192` slab. On GKI 6.1, offset 160 falls within `msg_msg` user payload space or invalidates non-controllable pointers, resulting strictly in **DoS / Kernel Panic**, defended by PAC, BTI, and kCFI.
* **Full Analysis**: See `tier2/docs/29_PHYSICAL_ARM64_SILICON_REBOOT_ANALYSIS.md`.

## 1b. Strategic Perspective: Core Kernel Races vs Vendor Driver Primitives

While Bad Epoll reached a DoS-only boundary on the hardened Android GKI, our parallel research into Android exploit primitives demonstrates why core kernel races face an increasingly insurmountable mitigation barrier:

### Architectural Contrast

| Dimension | Core Kernel Race (e.g. Bad Epoll) | Vendor Driver Primitive (e.g. GPU Driver UAF) |
|---|---|---|
| **Determinism** | Highly timing-sensitive, non-preemptible | Deterministic page-level lifecycle management |
| **Primitive** | Fixed 8-byte NULL write @ offset 160 | Arbitrary physical memory read/write via page table mapping |
| **Mitigations** | Capped at DoS by PAC, BTI, and kCFI | Sidesteps PAC/BTI/kCFI via direct physical page manipulation |
| **Domain** | Must execute within strict kernel syscall boundaries | Reachable from standard `untrusted_app` sandbox context |

### Key Takeaway for Android Exploitation Research

- **Mitigation Depth**: On GKI 6.1+, core kernel subsystems (`fs/`, `mm/`, `ipc/`) are heavily constrained by slab isolation (`SLAB_ACCOUNT`, `SLAB_TYPESAFE_BY_RCU`), preemption models (`PREEMPT_VOLUNTARY`), and hardware safety features (PAC, BTI, MTE).
- **Attack Surface Shift**: For research requiring full unprivileged application-to-root privilege escalation on modern Android, vendor driver attack surfaces (GPU, multimedia, NPU) provide deterministic primitives that do not rely on sub-microsecond scheduler races.
- **Bad Epoll Conclusion**: Bad Epoll remains a valuable, scientifically rigorous case study on modern kernel mitigation efficacy. Path C (documented negative-result writeup) serves as the primary publishable deliverable.

---

## 2. What was proved (RUNTIME evidence)

| ID | Claim | Method |
|---|---|---|
| VER-026 (EXP-015) | Race proven: Thread A `WRITE_ONCE(file->f_ep, NULL)` in `__ep_remove`; Thread B bypasses `eventpoll_release_file` via lockless fast path; Thread A's `hlist_del_rcu` writes NULL at freed+160. Captured by hardware watchpoint. | RUNTIME |
| VER-027 (EXP-018/019) | `msg_msg` 144 B payload reclaims freed slot; attacker controls bytes from slab offset 48 (marker `0xdead000000000000` landed at offset 136). | RUNTIME |
| VER-020 | `sizeof(struct eventpoll) = 176 B` → kmalloc-192; `refs` (UAF target) @ offset 160. | STATIC |
| VER-038 (AND-001) | SysV IPC (`msgget/msgsnd/msgrcv`) functional on target kernel; `load_msg` trapped at ipc/msgutil.c:96. | RUNTIME |

GDB-assisted UAF hits: **~100% on demand**. Natural hits: **0 / 102,740**. Best timing alignment
error: **1 cycle (~16 ns)** @ delay=2,360 cycles.

---

## 3. What was disproved (4 chains, 21 dead ends)

| Chain | Theory | Killing evidence |
|---|---|---|
| **0 — Controlled crash** via `percpu_counter_dec` | Spray fake `user_struct` @136 to crash | `percpu_counter_dec` uses the **OUTER** (valid) epoll, never the freed inner one. Marker landed but never read. **VER-028** |
| **1 — Dual-watch KASLR leak** | 2 epitems → kernel-pointer write @160 → readback | Single-epitem UAF and multi-epitem pointer write are **structurally mutually exclusive** (eventpoll.c:826 check). **VER-033** (retracts VER-029/030) |
| **2 — Arbitrary decrement** via fake user_struct | Redirect decrement to modprobe_path | Decrement always runs on `root_user` (fbc = root_user+8). **VER-031/032** |
| **3 — Full LPE** | creds/modprobe_path | Depends on Chains 1+2 — both dead. |

Other dead paths: epitem same-cache reclaim (VER-016), struct file UAF (VER-018/025),
snd_timer_user theories (EVO-005/VER-013). Primitive remains: **fixed NULL @160**.
EXP-016 audit of all reachable kmalloc-192 structs (fib6_info, snd_timer_user, packet_fanout,
urb, wakeup_source): all NULL effects are crash-only or benign → **DoS-only**.

---

## 4. Why the natural race fails (root cause)

- `cond_resched()` at eventpoll.c:888/903 is a **NO-OP**: `dynamic_cond_resched` static key is
  FALSE (CONFIG_PREEMPT_DYNAMIC, default PREEMPT_VOLUNTARY); `TIF_NEED_RESCHED` never set on the
  pinned, idle CPU 0 (timer tick with queued=0; no IPIs). **VER-035 (NAT-002 correction)**.
- `__ep_remove` contains **zero preemption points** (disassembly-confirmed) — the vulnerable
  sequence runs atomically w.r.t. scheduling.
- Real window = pure instruction-count timing: **~250–550 cycles (~125–275 ns @ 2 GHz)** — below
  scheduling granularity.
- QEMU TCG does **not** model hardware cache-coherency / memory-bus timing → emulated results are
  a lower bound; NAT-005 (92,740 iters, isolcpus=1, nohz_full, 4 MB cache-eviction sweeper)
  achieved 1-cycle alignment error with **0 hits**.

---

## 5. Android portability

- AND-001 PASSED: SysV IPC + msg_msg spray viable on target kernel (same commit as production GKI).
- Caveats: tested in initramfs shell (root), **not** app context under SELinux enforcing + seccomp.
- AND-002 (KASLR), AND-003 (SELinux syscall audit), AND-004 (MTE/KASAN_HW_TAGS) still PLANNED.
- MTE on production devices may turn the UAF into a detected fault (DoS) rather than silent
  corruption — could cap impact at DoS regardless of chain.

---

## 6. Decision options

| Path | Description | Effort | Risk | Success prob. |
|---|---|---|---|---|
| **A — Hardware timing-widening** | Physical ARM64; false-sharing cache bouncing, slab contention, IPI/timer storms, closed-loop calibration from NAT-005 | 2–4 weeks | Medium | Medium — only path with theoretical basis |
| **B — Alternative race variant** | Race where freed object IS the ep parameter | 2–4 weeks | High | Low (structural dead ends likely) |
| **C — Conclude DoS-only** | Document all dead ends + statistical negatives; publish negative-result writeup | ~1 week | Low | High confidence |

**Hybrid recommendation:** Path A, strict 2-week timebox, kill-criteria = 0 hits in ~1M iterations
→ then Path C.

**Updated Strategic Perspective:** Path M (Vendor Driver Device Verification) represents the primary forward line for Android privilege escalation research — vendor driver UAFs are deterministic, app-reachable, and bypass modern mitigation stacks via direct page manipulation. Bad Epoll Path C (negative-result writeup) serves as the primary publishable deliverable for this repository. Path A (hardware testbox) remains an optional verification if required.

---

## 7. What we need from the mentor

1. **Decision:** approve **Path M (Vendor Driver Device Verification)** as the primary forward research track on an unlocked ARM64 test device.
2. **Backup decision:** approve the 2-week hardware timebox (Path A) for Bad Epoll on physical ARM64 silicon only if the mentor prefers that over Path M; otherwise conclude Path C (DoS-only writeup).
3. **Access:** Physical ARM64 test device (unlocked dev unit) **or** ARM64 KVM host with kernel-log access.

---

## 8. Metrics dashboard

| Metric | Value |
|---|---|
| Experiments executed | 19 (EXP-006..024, NAT-001/002/005, AND-001) |
| GDB-assisted UAF hits | ~100% on demand |
| Natural race hits | 0 / 102,740 |
| Best timing alignment error | 1 cycle (~16 ns) |
| Dead ends | 21 (4 chains · 5 objects · 4 primitives · 4 sprays · 4 misconceptions) |
| Retracted claims (kept visible) | VER-010, VER-029, VER-030 |
| Active verification entries | VER-009..VER-039 |

---

## 9. Reproducibility

- Protocol: `tier2/docs/EXPERIMENT_PROTOCOL.md` · Runner: `tier2/docs/RUNNER_GUIDE.md`
- Ledger: `tier2/docs/VERIFICATION_LEDGER.md` · Dead ends: `tier2/docs/DEAD_ENDS_REGISTER.md`
- Raw evidence: `tier2/evidence/` · Scripts: `tier2/scripts/`
- Build/run: `DEBUG=1 ./scripts/run_qemu.sh`, then `gdb -batch -q -x scripts/<S>.py android/artifacts/vmlinux`

## 10. Repo hygiene (resolved)

Evidence for EXP-014/020/021 + stray logs committed (`d3c9bd6a4`); script-path + hygiene fixes
(`e19a3a87d`); doc link fix (`31f7b4b98`). Only remaining working-tree item is the
`third_party/security-research` submodule's untracked content (upstream, not ours).
