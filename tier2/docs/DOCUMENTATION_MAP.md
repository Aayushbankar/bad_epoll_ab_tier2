# Tier 2 Documentation Architecture Map

This sitemap organizes the 54 documentation files in `tier2/docs/` into 15 authoritative domains.

---

## 1. Navigation & Sitemaps
- [00_OVERVIEW.md](file:///mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/docs/00_OVERVIEW.md): High-level repository entry point and goals.
- [DOCUMENTATION_MAP.md](file:///mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/docs/DOCUMENTATION_MAP.md): (This file) Complete architectural sitemap.
- [tier2_demo_guide.md](file:///mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/docs/tier2_demo_guide.md): Walkthrough guide for executing Tier2 demonstrations.

---

## 2. Machine-Parseable Ledgers & Indexes
- [VERIFICATION_LEDGER.md](file:///mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/docs/VERIFICATION_LEDGER.md): Single Source of Truth mapping claims to raw log evidence.
- [EXPERIMENT_INDEX.md](file:///mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/docs/_archive_pre_reset/EXPERIMENT_INDEX.md): Index of test reproducers, GDB scripts, and shell launchers.
- [KNOWLEDGE_EVOLUTION.md](file:///mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/docs/_archive_pre_reset/KNOWLEDGE_EVOLUTION.md): Tracking assumptions, discoveries, and lessons learned.

---

## 3. Progress Tracking (Single Source of Truth)
- [CURRENT_PROGRESS.md](file:///mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/docs/CURRENT_PROGRESS.md): **SSOT** for project status and completed milestones.
- [01_ROADMAP.md](file:///mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/docs/01_ROADMAP.md): Redirect -> `CURRENT_PROGRESS.md`
- [02_CHECKLIST.md](file:///mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/docs/02_CHECKLIST.md): Redirect -> `CURRENT_PROGRESS.md`
- [PROJECT_STATE.md](file:///mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/docs/PROJECT_STATE.md): Redirect -> `CURRENT_PROGRESS.md`
- [REPRODUCTION_STATUS.md](file:///mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/docs/REPRODUCTION_STATUS.md): Redirect -> `CURRENT_PROGRESS.md`

---

## 4. Environment & Toolchain Specifications
- [ENVIRONMENT.md](file:///mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/docs/ENVIRONMENT.md): **SSOT** for toolchain, NDK Clang, QEMU, and kernel boot specs.
- [04_INSTALLATION.md](file:///mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/docs/04_INSTALLATION.md): Setup procedure reference.
- [TOOLCHAIN.md](file:///mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/docs/TOOLCHAIN.md): Redirect -> `ENVIRONMENT.md`

---

## 5. Research & Kernel Source Analysis
- [KERNEL_RESEARCH_DB.md](file:///mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/docs/_archive_pre_reset/KERNEL_RESEARCH_DB.md): **SSOT** for kernel offsets, struct layouts, and symbol maps.
- [UAF_PRIMITIVE_ANALYSIS.md](file:///mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/docs/_archive_pre_reset/UAF_PRIMITIVE_ANALYSIS.md): Source breakdown of eventpoll UAF, dual-watch topology, and stale write.
- [KERNEL_EXECUTION_FLOW.md](file:///mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/docs/KERNEL_EXECUTION_FLOW.md): Call graph mappings (`__mutex_lock_common`, `__ep_remove`).
- [SUBSYSTEM_DEPENDENCIES.md](file:///mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/docs/SUBSYSTEM_DEPENDENCIES.md): Inter-subsystem map between eventpoll, slab, and sound timer.

---

## 6. Experiments & Execution Guides
- [03_RESEARCH_LOG.md](file:///mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/docs/03_RESEARCH_LOG.md): Daily research log.
- [gdb_race_experiment.md](file:///mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/docs/_archive_pre_reset/gdb_race_experiment.md): GDB race experiment setup and spin patching guide.
- [MANUAL_EXECUTION_GUIDE.md](file:///mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/docs/MANUAL_EXECUTION_GUIDE.md): Interactive debugging guide.
- [RUNTIME_WORKFLOW.md](file:///mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/docs/RUNTIME_WORKFLOW.md): Execution loop workflow.

---

## 7. Evidence & Validation Standards
- [EVIDENCE_REQUIREMENTS.md](file:///mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/docs/EVIDENCE_REQUIREMENTS.md): Proof standards.
- [REPRODUCIBILITY.md](file:///mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/docs/REPRODUCIBILITY.md): Determinism guidelines.
- [RUNTIME_VALIDATION.md](file:///mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/docs/RUNTIME_VALIDATION.md): Pre/post memory state proof protocols.

---

## 8. Reference Material
- [COMMAND_REFERENCE.md](file:///mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/docs/COMMAND_REFERENCE.md): Shell and GDB command syntax reference.
- [EXPERIMENT_LOG_TEMPLATE.md](file:///mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/docs/EXPERIMENT_LOG_TEMPLATE.md): Markdown template for logging experiments.
