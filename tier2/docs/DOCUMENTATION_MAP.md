# Tier 2 Documentation Architecture Map

This sitemap organizes the 54 documentation files in `tier2/docs/` into 15 authoritative domains.

---

## 1. Navigation & Sitemaps
- [00_OVERVIEW.md](00_OVERVIEW.md): High-level repository entry point and goals.
- [DOCUMENTATION_MAP.md](DOCUMENTATION_MAP.md): (This file) Complete architectural sitemap.
- [tier2_demo_guide.md](tier2_demo_guide.md): Walkthrough guide for executing Tier2 demonstrations.

---

## 2. Machine-Parseable Ledgers & Indexes
- [VERIFICATION_LEDGER.md](VERIFICATION_LEDGER.md): Single Source of Truth mapping claims to raw log evidence.
- [EXPERIMENT_INDEX.md](_archive_pre_reset/EXPERIMENT_INDEX.md): Index of test reproducers, GDB scripts, and shell launchers.
- [KNOWLEDGE_EVOLUTION.md](_archive_pre_reset/KNOWLEDGE_EVOLUTION.md): Tracking assumptions, discoveries, and lessons learned.

---

## 3. Progress Tracking (Single Source of Truth)
- [CURRENT_PROGRESS.md](CURRENT_PROGRESS.md): **SSOT** for project status and completed milestones.
- [01_ROADMAP.md](01_ROADMAP.md): Redirect -> `CURRENT_PROGRESS.md`
- [02_CHECKLIST.md](02_CHECKLIST.md): Redirect -> `CURRENT_PROGRESS.md`
- [PROJECT_STATE.md](PROJECT_STATE.md): Redirect -> `CURRENT_PROGRESS.md`
- [REPRODUCTION_STATUS.md](REPRODUCTION_STATUS.md): Redirect -> `CURRENT_PROGRESS.md`

---

## 4. Environment & Toolchain Specifications
- [ENVIRONMENT.md](ENVIRONMENT.md): **SSOT** for toolchain, NDK Clang, QEMU, and kernel boot specs.
- [04_INSTALLATION.md](04_INSTALLATION.md): Setup procedure reference.
- [TOOLCHAIN.md](TOOLCHAIN.md): Redirect -> `ENVIRONMENT.md`

---

## 5. Research & Kernel Source Analysis
- [KERNEL_RESEARCH_DB.md](_archive_pre_reset/KERNEL_RESEARCH_DB.md): **SSOT** for kernel offsets, struct layouts, and symbol maps.
- [UAF_PRIMITIVE_ANALYSIS.md](_archive_pre_reset/UAF_PRIMITIVE_ANALYSIS.md): Source breakdown of eventpoll UAF, dual-watch topology, and stale write.
- [KERNEL_EXECUTION_FLOW.md](KERNEL_EXECUTION_FLOW.md): Call graph mappings (`__mutex_lock_common`, `__ep_remove`).
- [SUBSYSTEM_DEPENDENCIES.md](SUBSYSTEM_DEPENDENCIES.md): Inter-subsystem map between eventpoll, slab, and sound timer.

---

## 6. Experiments & Execution Guides
- [03_RESEARCH_LOG.md](03_RESEARCH_LOG.md): Daily research log.
- [gdb_race_experiment.md](_archive_pre_reset/gdb_race_experiment.md): GDB race experiment setup and spin patching guide.
- [MANUAL_EXECUTION_GUIDE.md](MANUAL_EXECUTION_GUIDE.md): Interactive debugging guide.
- [RUNTIME_WORKFLOW.md](RUNTIME_WORKFLOW.md): Execution loop workflow.

---

## 7. Evidence & Validation Standards
- [EVIDENCE_REQUIREMENTS.md](EVIDENCE_REQUIREMENTS.md): Proof standards.
- [REPRODUCIBILITY.md](REPRODUCIBILITY.md): Determinism guidelines.
- [RUNTIME_VALIDATION.md](RUNTIME_VALIDATION.md): Pre/post memory state proof protocols.

---

## 8. Reference Material
- [COMMAND_REFERENCE.md](COMMAND_REFERENCE.md): Shell and GDB command syntax reference.
- [EXPERIMENT_LOG_TEMPLATE.md](EXPERIMENT_LOG_TEMPLATE.md): Markdown template for logging experiments.
