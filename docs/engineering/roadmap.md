# Project Roadmap

## Tier 1
**Current Environment Analysis and Reverse Engineering.**
* **Status:** Complete (with analysis).
* **Summary:** Attempted to run the public CVE-2026-46242 PoC on a custom-compiled local QEMU kernel. The effort resulted in cascading environmental failures due to missing instructions (`rdtscp`), shifted struct offsets, missing ROP gadgets, and drastically different race timing windows.

↓

## Tier 1.5
**Recreate the Original KernelCTF Environment and Validate Assumptions.**
* **Status:** Next Phase.
* **Summary:** Obtain the exact `bzImage`, `vmlinux`, and `.config` from the original KernelCTF target database. We will execute the PoC against this known-good baseline to validate that the exploit logic functions as intended and to eliminate custom compilation variables. 

↓

## Tier 2
**Adaptation for Android (Architectural and Environmental Differences).**
* **Status:** Planned.
* **Summary:** Once the baseline is validated, evaluate what it takes to port this attack to an Android kernel (e.g., GKI). Analyze the impact of ARM64 vs x86_64 architectures, Scudo/MTE vs SLUB allocators, and hardware mitigations (PAC/BTI) on the UAF and cross-cache primitives.

↓

## Tier 3
**Final Knowledge Base and Documentation.**
* **Status:** Planned.
* **Summary:** Compile all reasoning, methodologies, lessons learned, and environmental comparisons into a comprehensive, audit-ready security research document. The focus remains on engineering discipline, determinism, and accurate technical constraints rather than simply producing an exploit.
