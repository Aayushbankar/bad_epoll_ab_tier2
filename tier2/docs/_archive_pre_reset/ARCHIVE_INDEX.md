# Archive Index

This document tracks all files that have been archived to preserve historical context while ensuring they do not pollute the active authoritative documentation.

| Archived Document | Date Archived | Reason for Archival |
|-------------------|---------------|---------------------|
| `ENGINEERING_READINESS_ASSESSMENT.md` | 2026-07-25 | Rendered obsolete by VER-013. The document outlined a path to Goal 2 based on the retracted assumption that the `wait_list` was corrupted and consumed in the `__mutex_lock_slowpath`. |
| `KMALLOC_192_TARGETS.md` | 2026-07-25 | Rendered obsolete by VER-011. The document assumed the UAF target object lived in `kmalloc-192`, but it has been verified that `struct epitem` resides in `kmalloc-128`. |
| `KNOWLEDGE_EVOLUTION.md` | 2026-07-23 (Pre-reset) | Archived during the verification reset as it contained a mix of true and fabricated claims regarding the UAF vulnerability. |
| `KERNEL_RESEARCH_DB.md` | 2026-07-23 (Pre-reset) | Archived during the verification reset; contained unverified offset assumptions and structs that were later proven inaccurate. |
| `UAF_PRIMITIVE_ANALYSIS.md` | 2026-07-23 (Pre-reset) | Archived during the verification reset. The document analyzed the UAF primitive based on the retracted `kmalloc-192` target and incorrect `NULL` write assumptions. |
| `gdb_race_experiment.md` | 2026-07-23 (Pre-reset) | Archived during the verification reset. Outlined an experiment based on retracted target constraints. |
| `EXPERIMENT_INDEX.md` | 2026-07-23 (Pre-reset) | Archived during the verification reset because it indexed experiments built upon retracted/unverified claims. |
| `VERIFICATION_LEDGER.md` | 2026-07-23 (Pre-reset) | The old ledger from before the verification reset. Archived because it contained unsupported claims and evidence linking to agent task logs. |
