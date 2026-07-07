# Tier 1 Environment Constants & Offsets

This document serves as a consolidated reference for all environment-specific variables, memory addresses, and struct offsets discovered during the Tier 1 QEMU execution of CVE-2026-46242. 

These values override the default generic exploit framework configurations.

## 1. Target Fingerprint
* **Kernel Build String (from `/proc/version`):** `Linux version 6.12.67 (legion@fedora)`
* **Target DB ID (kernelctf.kxdb):** `lts-6.12.67`

## 2. KASLR & Memory Addresses
* **KASLR Status:** Disabled (`nokaslr`)
* **Hardcoded Kernel Base:** `0xffffffff81000000`
* **Failed Stack Pivot Gadget (AT&T syntax trap):** `0xffffffff820d6525`

## 3. `task_struct` Offsets (Mapped via GDB)
Google's default offsets did not match our local build. These are the reversed offsets for our specific `vmlinux` binary.

| Field | Google's Default Offset | Our Custom Offset | Purpose |
| :--- | :--- | :--- | :--- |
| `comm` | 1928 | **1840** | Reading the command name to verify AAR |
| `files` | 1984 | **1896** | Locating open file descriptors |
| `children` | 1472 | **1384** | Walking the process tree |
| `sibling` | 1488 | **1400** | Walking the process tree |

## 4. Race Condition Calibration Constants
The nested QEMU environment required custom time threshold constants for the `epoll` vs `timerfd` race condition.

* **`RACE_DUP_CLOSE_ITERS` (Cache Bouncing Delay):** **20** (Original: 250)
* **`RACE_AHEAD_HI` (Timer Interrupt Search Window):** **10000ns** (Original: 4000ns)
* **Timeout Window:** **600 seconds** (Original: 300 seconds)
