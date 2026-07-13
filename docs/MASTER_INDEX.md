# Master Documentation Index

This index serves as the centralized table of contents for all engineering reports, timelines, architectural diagrams, and historical audits contained within the `docs/` repository.

## Architecture & Vulnerability (`docs/architecture/`)
- [VULNERABILITY.md](architecture/VULNERABILITY.md): Deep-dive into the `CVE-2026-46242` Bad Epoll Use-After-Free condition.

## Checkpoints & Audits (`docs/checkpoints/`)
**Tier 1 Complete Verification Suite:**
- [01_WORKING_STATE.md](checkpoints/tier1_complete/01_WORKING_STATE.md): Final host and environmental state parameters.
- [02_RUNTIME_LOG.md](checkpoints/tier1_complete/02_RUNTIME_LOG.md): Full raw QEMU runtime logs.
- [03_REPRODUCTION_GUIDE.md](checkpoints/tier1_complete/03_REPRODUCTION_GUIDE.md) / [AUDIT_REPRODUCTION.md](checkpoints/tier1_complete/AUDIT_REPRODUCTION.md): Step-by-step reproduction instructions.
- [04_ENGINEERING_TIMELINE.md](checkpoints/tier1_complete/04_ENGINEERING_TIMELINE.md) / [AUDIT_TIMELINE.md](checkpoints/tier1_complete/AUDIT_TIMELINE.md): Chronological development timeline.
- [05_FAILURE_HISTORY.md](checkpoints/tier1_complete/05_FAILURE_HISTORY.md): Catalog of developmental roadblocks.
- [06_RUNTIME_EVIDENCE_INDEX.md](checkpoints/tier1_complete/06_RUNTIME_EVIDENCE_INDEX.md) / [AUDIT_EVIDENCE.md](checkpoints/tier1_complete/AUDIT_EVIDENCE.md): File paths for physical evidence dumps.
- [07_ARCHITECTURE.md](checkpoints/tier1_complete/07_ARCHITECTURE.md): Exploit component interactions.
- [08_PORTING_GUIDE.md](checkpoints/tier1_complete/08_PORTING_GUIDE.md): Methodologies for porting via database regeneration.
- [09_LESSONS_LEARNED.md](checkpoints/tier1_complete/09_LESSONS_LEARNED.md): Engineering retrospective.
- [10_EVIDENCE_MANIFEST.md](checkpoints/tier1_complete/10_EVIDENCE_MANIFEST.md) / [EVIDENCE_CHECKLIST.md](checkpoints/tier1_complete/EVIDENCE_CHECKLIST.md): Verification matrix.
- [FINAL_REPORT.md](checkpoints/tier1_complete/FINAL_REPORT.md): The official professional engineering write-up.
- [RELEASE_CHECKPOINT.md](checkpoints/tier1_complete/RELEASE_CHECKPOINT.md): Top-level project status summary.
- [AUDIT_REPOSITORY.md](checkpoints/tier1_complete/AUDIT_REPOSITORY.md): Dependency mappings and environmental definitions.

## Engineering Notes & Guides (`docs/engineering/`)
- [ENGINEERING_KT.md](engineering/ENGINEERING_KT.md): Comprehensive Knowledge Transfer document.
- [PORTING_WORKFLOW.md](engineering/PORTING_WORKFLOW.md): Operational guide for updating target kernels.
- [PROJECT_RECOVERY_GUIDE.md](engineering/PROJECT_RECOVERY_GUIDE.md): Fail-safe restore and troubleshooting sequences.
- [EXPLOIT_WALKTHROUGH.md](engineering/EXPLOIT_WALKTHROUGH.md): Flowchart mapping of the exploit lifecycle.
- [roadmap.md](engineering/roadmap.md): Strategic roadmap spanning Tier 1 to Tier 3.
- [tier1_5_environment_recreation.md](engineering/tier1_5_environment_recreation.md): Details on aligning the `libxdk` payload parser with upstream.

## Environment Specifications (`docs/environment/`)
- [ENVIRONMENT_CONSTANTS.md](environment/ENVIRONMENT_CONSTANTS.md): Expected static values for the Tier 1 QEMU execution.
- [ENVIRONMENT_REBUILD_GUIDE.md](environment/ENVIRONMENT_REBUILD_GUIDE.md): Baseline restoration protocols.

## Runtime Evidence (`docs/evidence/`)
- [ROP_PROVENANCE_AUDIT.md](evidence/ROP_PROVENANCE_AUDIT.md): Detailed proof mapping the `0xffffffff810001bd` failure natively to the database offset desync.

## Mentor Status & Sprint Logs (`docs/mentor/`)
- [MENTOR_STATUS_REPORT.md](mentor/MENTOR_STATUS_REPORT.md): Consolidated brief for mentorship review.
- [PROGRESS.md](mentor/PROGRESS.md) / [PROGRESS_ASSESSMENT.md](mentor/PROGRESS_ASSESSMENT.md): Ongoing developmental task tracking.
- [SPRINT_RETROSPECTIVE.md](mentor/SPRINT_RETROSPECTIVE.md) / [tier1_retrospective.md](mentor/tier1_retrospective.md): Sprint-level review materials.
- [logbook.md](mentor/logbook.md) / [learning_gaps.md](mentor/learning_gaps.md): Personal developmental journaling.

## Reports (`docs/reports/`)
- [REPRODUCIBILITY_REPORT.md](reports/REPRODUCIBILITY_REPORT.md): Results validating functional environmental parity.
- [TIER1_RUN_REPORT.md](reports/TIER1_RUN_REPORT.md): Empirical run data from early stage deployments.
- [EXPLOIT_FRAGILITY_REPORT.md](reports/EXPLOIT_FRAGILITY_REPORT.md): Analysis of the race condition scheduling instability.

## Archive (`docs/archive/`)
- [CONTEXT.md](archive/CONTEXT.md): Legacy overview.
- [CMDS_RUN.txt](archive/CMDS_RUN.txt): Deprecated command logs.
- [PROJECT_TIMELINE_AND_AUDIT.md](archive/PROJECT_TIMELINE_AND_AUDIT.md): Replaced by `AUDIT_TIMELINE.md`.
- [ENVIRONMENT_CONSTANTS.md.bak](archive/ENVIRONMENT_CONSTANTS.md.bak): Deprecated environmental variables.
