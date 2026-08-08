# TIER 2 EXPLOITABILITY ASSESSMENT — COMPLETE REPORT

**CVE-2026-46242 ("Bad Epoll")** — Use-After-Free in the Linux epoll subsystem on Android ARM64 GKI
**Branch:** tier2-android-port @ cc0dc7754 (verified on origin)
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

## 1b. ⭐ NEW FINDINGS — parallel CVE primitive hunt (2026-08-07): a better path exists

While Bad Epoll reached DoS-only, a parallel hunt scored **CVE-2025-0072 (Arm Mali GPU kernel
driver UAF)** as the top candidate — **58/100** — a **deterministic, app→root exploit with a
complete public PoC** that sidesteps exactly what killed Bad Epoll (a schedulable race).

### The shortlist (weighted: PubExploit×3, App→Root×3, DeviceBase×2, Reliability×2, Determinism×1, Freshness×1)

| Rank | Candidate | Score | Class | Why it matters |
|---|---|---|---|---|
| 1 | **CVE-2025-0072 (Mali GPU UAF)** | **58** | Deterministic UAF, public exploit | app → arb kernel R/W → SELinux off → **root**; MTE/PAC/BTI/kCFI bypassed |
| 2 | CVE-2025-6349/8045 (Mali CSF) | 45 | Newest drivers (r53p0–r54p1) | Same primitive, still-unpatched base; no PoC |
| 3 | Dirty Frag 43284/43500 | 44 | Kernel UAF (splice+XFRM) | Public root exploit; weak Android reach |
| 4 | CVE-2026-21385 (Qualcomm/Adreno) | 42 | In-the-wild graphics 0-day | 230+ chipsets; closed-source, no PoC |
| 5 | Chronomaly 38352 | 42 | POSIX CPU timers UAF | Public root exploit; 32-bit Android only |
| 6 | Framework 48595 | 39 | Framework EoP | App→System stage; kernel primitive still needed |
| 7 | Bad Epoll 46242 (current) | 36 | Race UAF | DoS-only on our config — the gap we're filling |
| 8 | CVE-2026-0163 (VPU, Pixel) | 27 | Pixel-only UAF | Incomplete-fix/sibling audit target |

### Why CVE-2025-0072 is the headline

- **Complete public exploit** (GitHub Security Lab / Man Yue Mo, GHSL-2024-356): `mali_userio.c`,
  `mem_read_write.c` (arb kernel R/W), `mempool_utils.c`. Proven from the **untrusted app domain**.
- **Deterministic page-level UAF** — not a schedulable race. Author: *"rarely fails; can be
  rerun."* This is the single biggest contrast with Bad Epoll.
- **Mitigations already defeated**: MTE bypass demonstrated on Pixel 8; direct physical-page
  writes sidestep PAC/BTI/kCFI without forging function pointers.
- **Affected**: Mali Valhall r29p0–r53p0 + 5th-gen r41p0–r53p0 = every Mali device 2020→mid-2025.
  Fixed in ASB 2025-05-05 + r54p0, but **GPU drivers are not OTA'd on non-Pixel OEMs** → the
  budget/legacy fleet stays vulnerable for life.
- **India device-base analysis (new)**: #2/#3 best-selling 2025 models (vivo Y29 5G, T4x 5G) +
  FY2024's Y28s/T3 Lite are all **Dimensity 6300 / Mali-G57**; MediaTek≈Mali = 44–52% of
  shipments, ~45% of the 740M-unit Indian fleet → **~300M+ Mali-class devices**. See
  `analysis/india_device_market_2026-08-07.md` in the hunt repo.

### What it changes for the Bad Epoll decision

- Path M (Mali) is now the **primary** line: verify CVE-2025-0072 on one real Mali device →
  port/adapt → app→root with SELinux off.
- If the target firmware moved past r53p0, siblings CVE-2025-6349/8045 provide a fresh
  still-unpatched primitive (R&D cost, no public PoC).
- Adreno/Qualcomm (CVE-2026-21385) is the volume fallback for Snapdragon devices (incl. Redmi
  14C — the one top-5 budget model NOT on Mali).
- Bad Epoll Path A timebox only if the mentor mandates it; Path C negative-result writeup stays
  as the honest publishable outcome.

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

**Updated by the hunt (§1b):** Path M (Mali CVE-2025-0072) supersedes Path A/B as the primary
forward line — it is deterministic, app-reachable, has a complete public exploit, and targets
~300M+ Indian Mali devices. Recommend: (1) confirm 0072 on one Mali device → port; (2) if target
firmware is post-r53p0, pivot to 6349/8045; (3) keep Bad Epoll Path C writeup as the publishable
deliverable. Path A only if the mentor explicitly requires it.

---

## 7. What we need from the mentor

1. **Decision:** approve **Path M (Mali device verification)** as the primary track — an unlocked
   ARM64 Mali test device (e.g. vivo Y29 5G / T4x 5G / Galaxy A16 5G, all Dimensity/Exynos Mali)
   or vendor kernel images for GDB — one device to confirm, then scale.
2. **Backup decision:** approve the 2-week hardware timebox (Path A) for Bad Epoll only if the
   mentor prefers that over Path M; otherwise conclude Path C (DoS-only writeup).
3. **Access:** ARM64 test device (unlocked dev unit, Mali SoC preferred) **or** ARM64 KVM host
   with kernel-log access.

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

## 11. CVE hunt evidence trail (for Path M work)

- Hunt repo: `repos/cve_primitive_hunt/` — `REPORT_2026-08-07.md` (candidate report & selection
  rationale), `scoring/round2_2026-08-07.md` (weighted matrix), `analysis/india_device_market_2026-08-07.md`
  (device-base analysis), `harvest/github/CVE-2025-0072_mali.md` (full dossier: mechanics, affected
  versions, tested build AP3A.241105.007, trigger ioctls KBASE_IOCTL_CS_QUEUE_BIND /
  CS_QUEUE_GROUP_TERMINATE).
- Key sources: GitHub Security Lab writeup (GHSL-2024-356), Arm bulletin 110465, ASB 2025-05-05
  (A-391928904*), NVD, Qualcomm March 2026 bulletin, Project Zero 2026-05 Pixel 10 VPU writeup,
  Counterpoint/IDC/TechInsights India trackers 2023–2026.
