# Final Repository Structure

This document outlines the final directory tree and the explicit purpose of every major directory mapped during the Tier 1 Checkpoint Repo Reorganization.

```text
├── archive
│   ├── CMDS_RUN.txt
│   ├── CONTEXT.md
│   ├── debug
│   │   ├── disasm.py
│   │   ├── find_any_pivot.py
│   │   ├── find_clean_jop.py
│   │   ├── find_clean_pivot.py
│   │   ├── find_clean_pivots.py
│   │   ├── find_final_gadgets.py
│   │   ├── find_gadgets_objdump_pivot2.py
│   │   ├── find_gadgets_objdump_pivot.py
│   │   ├── find_gadgets_objdump.py
│   │   ├── find_gadgets.py
│   │   ├── find_jop2.py
│   │   ├── find_jop_objdump.py
│   │   ├── find_jop.py
│   │   ├── find_mov_rsp_rdi_ret.py
│   │   ├── find_mov_rsp_rdx.py
│   │   ├── find_pivot2.py
│   │   ├── find_pivot_2_gadget.py
│   │   ├── find_pivot_again.py
│   │   ├── find_pivot.py
│   │   ├── find_rdi_pivot.py
│   │   ├── get_offsets2.py
│   │   ├── get_offsets.py
│   │   ├── get_pivots.py
│   │   ├── get_real_pivots2.py
│   │   ├── get_real_pivots.py
│   │   ├── patch_exploit2.py
│   │   ├── patch_exploit.py
│   │   ├── patch_exploit_rdx.py
│   │   ├── rop_extractor2.py
│   │   ├── test_kaslr2.sh
│   │   ├── test_kaslr.sh
│   │   ├── test_pivot.py
│   │   ├── test_rop.py
│   │   └── trace.py
│   ├── docs
│   │   ├── 01_WORKING_STATE.md
│   │   ├── 02_RUNTIME_LOG.md
│   │   ├── 03_REPRODUCTION_GUIDE.md
│   │   ├── 04_ENGINEERING_TIMELINE.md
│   │   ├── 05_FAILURE_HISTORY.md
│   │   ├── 06_RUNTIME_EVIDENCE_INDEX.md
│   │   ├── 07_ARCHITECTURE.md
│   │   ├── 08_PORTING_GUIDE.md
│   │   ├── 09_LESSONS_LEARNED.md
│   │   └── 10_EVIDENCE_MANIFEST.md
│   ├── ENVIRONMENT_CONSTANTS.md.bak
│   ├── PROJECT_TIMELINE_AND_AUDIT.md
│   ├── REASON.md
│   └── scripts
│       ├── boot.sh
│       ├── disasm.py
│       ├── find_any_pivot.py
│       ├── find_clean_jop.py
│       ├── find_clean_pivot.py
│       ├── find_clean_pivots.py
│       ├── find_final_gadgets.py
│       ├── find_gadgets_objdump_pivot2.py
│       ├── find_gadgets_objdump_pivot.py
│       ├── find_gadgets_objdump.py
│       ├── find_gadgets.py
│       ├── find_jop2.py
│       ├── find_jop_objdump.py
│       ├── find_jop.py
│       ├── find_mov_rsp_rdi_ret.py
│       ├── find_mov_rsp_rdx.py
│       ├── find_pivot2.py
│       ├── find_pivot_2_gadget.py
│       ├── find_pivot_again.py
│       ├── find_pivot.py
│       ├── find_rdi_pivot.py
│       ├── find_real_pivot.py
│       ├── get_kernel_rop_0.py
│       ├── get_offsets2.py
│       ├── get_offsets.py
│       ├── get_pivots.py
│       ├── get_real_pivots2.py
│       ├── get_real_pivots.py
│       ├── patch_exploit2.py
│       ├── patch_exploit.py
│       ├── patch_exploit_rdx.py
│       ├── rop_extractor2.py
│       ├── rop_extractor.py
│       ├── run_gdb_dump.sh
│       ├── run_gdb_interactive.sh
│       ├── run_qemu_gdb.sh
│       ├── run_qemu_interactive.py
│       ├── run_qemu_interactive.sh
│       ├── start_qemu.sh
│       ├── test_kaslr2.sh
│       ├── test_kaslr.sh
│       ├── test_pivot.py
│       ├── test_rop.py
│       ├── trace_gdb.py
│       ├── trace.py
│       └── trace_qemu.py
├── article
│   └── draft.md
├── artifacts
│   ├── binaries
│   │   ├── bzImage
│   │   └── vmlinux
│   ├── databases
│   │   └── target_db.kxdb
│   ├── generated
│   │   ├── btf.json
│   │   ├── kernel_pages.txt
│   │   ├── rop_actions.json
│   │   ├── rp++.txt
│   │   ├── stack_pivots.json
│   │   ├── structs.json
│   │   ├── symbols.txt
│   │   └── version.txt
│   ├── hashes
│   │   ├── hashes.txt
│   │   └── SHA256SUMS
│   ├── logs
│   │   ├── expect.log
│   │   ├── gdb_rop.log
│   │   ├── gdb_trace_dump.log
│   │   ├── gdb_trace.log
│   │   ├── qemu_output.log
│   │   └── qemu_panic.log
│   ├── runtime
│   ├── screenshots
│   └── traces
├── docs
│   ├── architecture
│   │   └── VULNERABILITY.md
│   ├── archive
│   │   ├── CMDS_RUN.txt
│   │   ├── CONTEXT.md
│   │   ├── ENVIRONMENT_CONSTANTS.md.bak
│   │   └── PROJECT_TIMELINE_AND_AUDIT.md
│   ├── checkpoints
│   │   └── tier1_complete
│   ├── engineering
│   │   ├── ENGINEERING_KT.md
│   │   ├── EXPLOIT_WALKTHROUGH.md
│   │   ├── PORTING_WORKFLOW.md
│   │   ├── PROJECT_RECOVERY_GUIDE.md
│   │   ├── roadmap.md
│   │   └── tier1_5_environment_recreation.md
│   ├── environment
│   │   ├── ENVIRONMENT_CONSTANTS.md
│   │   ├── ENVIRONMENT_REBUILD_GUIDE.md
│   │   └── HOWTO_RUN_TIER1_5.md
│   ├── evidence
│   │   └── ROP_PROVENANCE_AUDIT.md
│   ├── MASTER_INDEX.md
│   ├── mentor
│   │   ├── learning_gaps.md
│   │   ├── logbook.md
│   │   ├── MENTOR_STATUS_REPORT.md
│   │   ├── PROGRESS_ASSESSMENT.md
│   │   ├── PROGRESS.md
│   │   ├── SPRINT_RETROSPECTIVE.md
│   │   └── tier1_retrospective.md
│   ├── reports
│   │   ├── EXPLOIT_FRAGILITY_REPORT.md
│   │   ├── REPRODUCIBILITY_REPORT.md
│   │   └── TIER1_RUN_REPORT.md
│   ├── runtime
│   │   ├── RUNTIME_EVIDENCE_AUDIT.md
│   │   └── RUNTIME_VALIDATION_REPORT.md
│   └── security-research
├── exploit
│   ├── tier1
│   │   ├── AI_HANDOFF_CHECKPOINT.md
│   │   ├── AI_HANDOFF.md
│   │   ├── any_pivots.txt
│   │   ├── boot.sh
│   │   ├── code.bin
│   │   ├── expect.log
│   │   ├── expect.pid
│   │   ├── exploit
│   │   ├── find_jop
│   │   ├── find_jop.c
│   │   ├── find_jop_chain
│   │   ├── find_jop_chain.c
│   │   ├── find_jop_rdi
│   │   ├── find_jop_rdi.c
│   │   ├── find_pivot
│   │   ├── find_pivot2
│   │   ├── find_pivot2.c
│   │   ├── find_pivot3
│   │   ├── find_pivot3.c
│   │   ├── find_pivot4
│   │   ├── find_pivot4.c
│   │   ├── find_pivot5
│   │   ├── find_pivot5.c
│   │   ├── find_pivot6
│   │   ├── find_pivot6.c
│   │   ├── find_pivot7
│   │   ├── find_pivot7.c
│   │   ├── find_pivot8
│   │   ├── find_pivot8.c
│   │   ├── find_pivot9
│   │   ├── find_pivot9.c
│   │   ├── find_pivot.c
│   │   ├── gadgets_pivot.txt
│   │   ├── gadgets.txt
│   │   ├── gdb_rop.log
│   │   ├── gdb_trace_dump.log
│   │   ├── gdb_trace.log
│   │   ├── initramfs.cpio
│   │   ├── initramfs_exploit.cpio
│   │   ├── initramfs_exploit_debug.cpio
│   │   ├── kernel_info.json
│   │   ├── patch_exploit.patch
│   │   ├── pivot_gadgets2.txt
│   │   ├── pivot_gadgets3.txt
│   │   ├── pivot_gadgets.txt
│   │   ├── PIVOT_RESEARCH_LOG.md
│   │   ├── PIVOT_SOLUTION.md
│   │   ├── qemu.log
│   │   ├── qemu_output.log
│   │   ├── qemu_panic.log
│   │   ├── qemu.pid
│   │   ├── README.md
│   │   ├── rootfs
│   │   ├── rootfs_build
│   │   ├── rootfs_debug
│   │   ├── rop_builder.cpp
│   │   ├── run_gdb_dump.sh
│   │   ├── run_gdb_interactive.sh
│   │   ├── run_gdb_script.gdb
│   │   ├── run_qemu_gdb.sh
│   │   ├── run_qemu_interactive.py
│   │   ├── run_qemu_interactive.sh
│   │   ├── start_qemu.sh
│   │   ├── test_dis
│   │   ├── test_dis.c
│   │   └── test_local_gdb.gdb
│   ├── tier1.5
│   │   ├── bzImage
│   │   ├── exploit
│   │   ├── initramfs.cpio
│   │   ├── rootfs
│   │   └── security-research
│   ├── tier2
│   │   └── README.md
│   └── tier3
│       └── README.md
├── README.md
├── repo_audit_list.txt
├── research
│   ├── get_kernel_rop_0.py
│   ├── rop_extractor.py
│   ├── trace_gdb.py
│   └── trace_qemu.py
├── scripts
│   ├── analysis
│   ├── build
│   ├── debug
│   ├── setup
│   │   ├── setup-tier1_5.sh
│   │   └── setup-tier1.sh
│   └── utilities
├── system_state.txt
└── third_party
    ├── bpftool-v7.7.0-amd64.tar.gz
    ├── linux-6.12.67
    ├── rp-lin-gcc.zip
    └── security-research
```

## Directory Definitions
- **archive/**: Retains all deprecated scripts, legacy command outputs, and duplicate documents. Nothing is deleted; it is stored here for historical reference.
- **artifacts/**: Frozen outputs generated during execution. Contains the final logs, binaries, databases, and structural traces.
- **docs/**: Comprehensive engineering hierarchy containing all reports, sprint reviews, architectures, and auditing checklists. The `MASTER_INDEX.md` resides here.
- **exploit/**: The operational tier directories containing active payloads, wrappers, and configuration files for specific environments.
- **research/**: Useful, active debugging and validation scripts (like Python GDB tracers) retained for future use.
- **scripts/**: Core repository automation scripts grouped by category (build, setup, debug).
- **third_party/**: External submodules and downloaded kernels (like linux-6.12.67 and rp++). Ownership remains external.
