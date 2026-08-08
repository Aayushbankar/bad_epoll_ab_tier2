# Repository Reorganization Changelog

## Overview
To transition this repository into a long-term engineering artifact suitable for Tier 2 evaluation, a comprehensive reorganization was performed. No engineering work, logs, scripts, or generated artifacts were deleted.

## Structural Changes
- **Created `archive/`**: Safely stores deprecated scripts, old generator copies, and redundant documentation without polluting active development trees.
- **Created `third_party/`**: Separated external upstream components (`linux-6.12.67`, `security-research`, `bpftool`, `rp++`) out of `exploit/tier1/` to clearly establish ownership bounds.
- **Created `docs/` Hierarchy**: Migrated all top-level and flat-structured markdown documents into explicit categorizations (`engineering/`, `reports/`, `checkpoints/`, `environment/`, `mentor/`, `evidence/`, `architecture/`, `archive/`).
- **Created `artifacts/` Hierarchy**: Migrated the flat backup copies of binaries, hashes, databases, and logs into categorized subdirectories (`binaries/`, `generated/`, `logs/`, `hashes/`, `databases/`).
- **Created `research/`**: Elevated essential debugging tools (`trace_qemu.py`, `trace_gdb.py`, `rop_extractor.py`) from the exploit directories into a global toolset.
- **Created `scripts/` Hierarchy**: Separated environment setup scripts out of root.

## Tier Renaming
- Renamed `exploit/tier1-linux-vm` to `exploit/tier1` to enforce strict naming consistency.
- Renamed `exploit/tier1_5-kernelctf-env` to `exploit/tier1.5`.
- Renamed `exploit/tier2-android-emulator` to `exploit/tier2`.
- Renamed `exploit/tier3-selinux-analysis` to `exploit/tier3`.

## File Movement Detail
- **Dependencies**: `linux-6.12.67` and `security-research` moved from `exploit/tier1/` to `third_party/`. 
- **Obsolete Scripts**: Dozens of `find_*.py`, `patch_exploit*.py`, and `test*.py` scripts used during the offset hallucination triage were moved to `archive/debug/`.
- **Logs & Artifacts**: Moved duplicated copies of the Tier 1 checkpoint docs and scripts found in `artifacts/docs/` and `artifacts/scripts/` to `archive/docs/` and `archive/scripts/` to eliminate redundancy while preserving history.
- **Documentation**: Root files like `learning_gaps.md`, `logbook.md`, and `tier1_retrospective.md` moved to `docs/mentor/`.

## Link Updates
- Internal paths inside shell scripts (e.g., `start_qemu.sh`, `run_qemu_gdb.sh`) were repointed from `linux-6.12.67/` to `../../third_party/linux-6.12.67/` to accommodate the structural shift.
- The root `README.md` was rewritten to serve as the master navigational hub, linking dynamically to the new `docs/MASTER_INDEX.md`.


---

## Repository Separation (2026-08-08)

The monorepo was split into two independent repositories:
- **`bad_epoll_ab_tier2`** (this repo): Tier 2 Android ARM64 GKI exploitability assessment
- **`bad-epoll-lab`** (archived on GitHub): Tier 1 x86_64 exploit on `main` branch

Changes:
- Branch `tier2-android-port` renamed to `main`
- Branch `tier1.5-investigation` deleted (local and remote)
- `exploit/tier1/` and `exploit/tier1.5/` removed from working tree
- Top-level Tier 1 artifacts removed (`.aux`, `.log`, `.out`, `.toc`, session log)
- Remote repointed from `Aayushbankar/bad-epoll-lab` to `Aayushbanker/bad_epoll_ab_tier2`
- All historical documentation preserved with migration annotations
- Full commit history (85 commits) retained
