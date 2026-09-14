# Session Resume & End-Goal Path (CVE-2026-46242 - Android ARM64)

> **Date:** 2026-09-13 | **Branch:** main | **HEAD:** 3a7a05c7f
> **Purpose:** Single file to resume this session later and drive it to the end goal (real QEMU PoC, then physical).
> Canonical live status always lives in CURRENT_PROGRESS.md (Rule 7).

## 1. End goal

A working, evidence-backed exploit PoC on android15-6.6-2025-10_r1 (Linux 6.6.102) inside QEMU, with real raw logs, portable to physical hardware later. End-to-end getuid()==0 via CVE-2026-46242 epoll UAF chain (survivor epoll + freed struct file -> cross-cache reclaim -> fdinfo AAR -> gated swaps_poll write).

## 2. Current progress (what is done)

### Phase 3 environment (EXP-030 / VER-072) - COMPLETE
- QEMU virt boot of android15-6.6-2025-10_r1 (6.6.102, commit 3dff304da0a6, build ab14202157) works via tier3/artifacts/Image + tier3/initramfs.cpio.
- __ep_remove verified unpinned: ldr x24,[x1,#48] (epi->ffd.file) with no epi_fget(); WRITE_ONCE(file->f_ep,NULL) present; eventpoll_release fast-path intact; fix symbols absent. (VER-070/072)
- Offsets: swaps_poll writes str w8,[x19,#112] (private_data+0x70); struct file private_data@+216, f_ep@+224. (VER-072/075/077 pending)
- Unprivileged uid=2000 priv-drop + /proc/self/fdinfo/<epfd> ino: AAR oracle confirmed live. (VER-072)
- Evidence: tier3/evidence/PHASE3_DAY1_SETUP_RESULTS.md, qemu_serial_certified_6.6.102.log, ep_remove_disasm_6.6.102.txt, ep_remove_certified.txt, ep_show_certified.txt.

### Stage-1 race (EXP-031 / VER-073/074/075) - primitive confirmed, farming blocked
- On 6.6.102 the vulnerable pattern is present but natural race does not fire under QEMU TCG: 0 wins in 1000, 5000+, and launch_ahead (500-6000ns) sweeps; ~15ms/iter.
- Race window in __ep_remove is 11 instructions (down from 18 on 6.1) - see tier3/evidence/ep_remove_certified.txt and EXP-031_RESULTS.md.
- Key decision: do NOT farm the natural race in QEMU TCG. Force Stage-1 deterministically via GDB state injection (Variant B) as validated on 6.1 in VER-058/060, then run Stage-2 (reclaim/write) genuinely.

## 3. Critical blocker discovered this session

**The certified/debug 6.6.102 Image has NO /dev/dma_heap/system** (serial: /sys/class/dma_heap/system/dev not found; vmlinux has dma_heap_add but zero system_heap* symbols). Result:
- The Stage-2 AAR + swaps_poll chain (needs a live-editable userspace-mapped page to overlay the freed filp slab page) cannot run on the current image - this is why prior runs look like reclaim miss (0/30 in EXP-029).
- Fix: build a 6.6.102 image with CONFIG_DMABUF_HEAPS_SYSTEM=y, OR use the verified no-rebuild fallback memfd_create() order-0 buddy pages (VER-046/049: 99.55% drain; memfd is live-editable via MAP_SHARED).

## 4. Decisions made

1. Target frozen: android15-6.6-2025-10_r1 (6.6.102). VER-071 proved every other android15-6.6 respin has the 8-commit fix; only pre-fix-r1-class devices are exploitable.
2. QEMU = correctness proof, not timing. No more TCG racing. Use deterministic GDB injection for Stage-1.
3. Stage-2 is the real unsolved problem (not the race): reliable cross-cache reclaim of the freed filp slab into a live-editable page. Prioritize that.
4. Housekeeping first, engine later: fix ledger (restore VER-073 - done), add EXP-031 to index (done), add VER-076/077/078 + EVO-039, refresh CURRENT_PROGRESS.md / PROJECT_STATE.md, then proposal.
5. Fix EVO-037 exit panic (leaked ep_uaf_waiter fds cause remove_wait_queue list_del panic at process exit) before any multi-shot unassisted run.

## 5. New info/facts this session (with citations)

| Fact | Source | Type |
|---|---|---|
| 6.6 filp slab: object_size 264, stride 320, order-0, 12 objs/slab | slab_check.log, EXP-031_RESULTS.md (VER-073) | RUNTIME |
| struct file f_ep@+224, private_data@+216 | ep_remove_certified.txt (str xzr,[x24,#224]), ep_show_certified.txt (ldr x19,[x1,#216]) | STATIC |
| swaps_poll = 0xffffffc0803fbc18, writes private_data+0x70 | GDB disassemble swaps_poll | STATIC |
| Stage-1 natural: 0/1000+, 0/5000+, 0 in sweeps (QEMU TCG) | tier3/evidence/qemu_stage1_*.log | RUNTIME |
| No system_heap in certified image; gki_defconfig has DMABUF_HEAPS=y but not DMABUF_HEAPS_SYSTEM | gki_defconfig + serial | STATIC |
| 6.1 baseline: 30/150k race wins but 0/30 reclaim, exit panic (EVO-037) | EXP-029_RESULTS.md, EXP-029_raw_serial.log:474-480 | RUNTIME |
| PROVEN 6.1 primitives to carry: forced survivor, dma-buf AAR, swaps_poll cred-zero | exp_hyp011_aar_gdb.py, exp_hyp012_gdb.py, test_hyp012.c (VER-058/060/061/062) | RUNTIME+STATIC |

## 6. Concrete next steps (to reach end goal)

1. Housekeeping (short): add ledger VER-076/077/078 + EVO-039; refresh CURRENT_PROGRESS.md + PROJECT_STATE.md; commit + push; verify git ls-remote origin main (Rule 10).
2. Write the proposal tier2/docs/PHASE3_QEMU_POC_PROPOSAL.md with concrete steps, citations, reasons (this session's ask).
3. Build/enable Stage-2 reclaim on 6.6.102: build system_heap image or port memfd AAR; port HYP-011/012 with 6.6 offsets (+216/+224, +0x70).
4. Deterministic Stage-1: GDB-forced file->f_ep=NULL at __fput on 6.6 (port exp_hyp008/011), confirm survivor + freed file.
5. Close the loop: cross-cache reclaim -> fdinfo AAR swapper/ gate -> gated swaps_poll zero of cred.uid/euid/fsuid -> getuid()==0.
6. Fix EVO-037 exit panic; then re-run unassisted to completion.
7. Physical (later): calibrate natural race timing (timerfd widening + launch-ahead) on real pre-fix Pixel-10-class silicon; QEMU can't prove timing.

## 7. Files to touch (fast reference)

- Docs: EXPERIMENT_INDEX.md, VERIFICATION_LEDGER.md, CURRENT_PROGRESS.md, PROJECT_STATE.md, new PHASE3_QEMU_POC_PROPOSAL.md.
- Engine ports: tier2/scripts/exp_hyp008_gdb.py, exp_hyp011_aar_gdb.py, exp_hyp012_gdb.py, test_hyp012.c, test_exp029_e2e.c, test_m7_spray.c.
- Evidence: tier3/evidence/EXP-031_RESULTS.md, PHASE3_DAY1_SETUP_RESULTS.md, ep_remove_certified.txt, ep_show_certified.txt, qemu_stage1_*.log, slab_check.log.
- Build facts: third_party/android15-6.6-2025-10_r1/arch/arm64/configs/gki_defconfig; ledger VER-046/049 (drain + memfd).

## 8. Rule 10 note (honest self-check)

- No commit/push of this session's docs yet; git status is not clean (pre-existing untracked evidence by design).
- git ls-remote origin main NOT run this session; push NOT done for new/edited docs. Say so explicitly. Index/ledger edits are uncommitted working-tree changes.
