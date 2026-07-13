# CVE-2026-46242: Bad Epoll

Welcome to the internal engineering repository for the "Bad Epoll" vulnerability (CVE-2026-46242). This repository houses the complete documentation, research, and functional exploits targeting this specific Linux kernel Use-After-Free condition.

## Project Overview
This project successfully ports the official kernelCTF exploit to a localized Fedora GCC-compiled `linux-6.12.67` environment. It provides a robust Jump-Oriented Programming (JOP) and Return-Oriented Programming (ROP) payload capable of fully bypassing KASLR and KPTI to achieve unprivileged root execution. 

## Repository Layout
This repository has been strictly engineered to provide a self-contained, reproducible artifact for long-term archival and further Tier 2 (Android) development. 

- **`docs/`**: The master repository for all technical reports, timelines, architectural diagrams, and engineering checklists. 
- **`exploit/`**: The tiered structure housing the localized exploit environments (Tier 1 through Tier 3).
- **`artifacts/`**: The frozen archival collection containing every generated database, binary, memory dump, and system trace collected during development.
- **`scripts/`**: Setup, utility, and build automation scripts.
- **`research/`**: Validated debugging tools, tracers, and memory extraction utilities.
- **`third_party/`**: External dependencies, including the raw `linux-6.12.67` kernel source tree and the upstream `security-research` generators.
- **`archive/`**: Deprecated scripts and redundant file copies retained strictly for historical preservation.

## Tier Overview
- **`exploit/tier1/`**: The primary localized Linux VM environment. Verified, stable, and completely self-contained. 
- **`exploit/tier1.5/`**: The kernelCTF recreation environment for upstream synchronization.
- **`exploit/tier2/`**: Prepared environment for Android ARM64 kernel porting.
- **`exploit/tier3/`**: Prepared environment for advanced SELinux enforcement analysis.

## Quick Start & Reproduction
To fully reproduce the Tier 1 environment from a fresh installation, consult the comprehensive [Reproduction Guide](docs/checkpoints/tier1_complete/AUDIT_REPRODUCTION.md). It details dependency installation, `target_db` regeneration, and VM execution.

## Evidence & Verification
The success of this port is backed by concrete runtime evidence (memory dumps, live GDB instruction traces, QEMU console outputs). A complete index is available in the [Runtime Evidence Index](docs/checkpoints/tier1_complete/AUDIT_EVIDENCE.md).

## Documentation Index
For a complete listing of all internal engineering reports, historical timelines, and vulnerability analyses, please see the [Master Index](docs/MASTER_INDEX.md).

## Future Work
The repository is frozen at the successful Tier 1 boundary. The next phase transitions into `exploit/tier2/` to evaluate the feasibility of porting the `epoll` Use-After-Free timing constraints to the Android Generic Kernel Image (GKI) utilizing ARM64 EL1 -> EL0 transition paradigms.
