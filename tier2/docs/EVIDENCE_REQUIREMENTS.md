# Evidence Requirements

To ensure Tier 2 remains as reproducible and scientifically rigorous as Tier 1, all engineering phases must produce immutable evidence. The following evidence artifacts must be collected and archived throughout the development lifecycle.

## Required Artifacts

### 1. Terminal Recordings
- `script` logs must capture every execution session.
- Output logs must include timing data (`script -t`).

### 2. Output Logs
- `adb logcat` output capturing the exploit trigger and any relevant userland context.
- Kernel panic traces (`dmesg` or emulator serial output).
- Clean boot logs (for diffing against polluted/crashed states).

### 3. Debugging Transcripts
- Remote `gdb-multiarch` or `pwndbg` session transcripts.
- Explicit register dumps (e.g., `info registers`, `x/20gx $sp`) at critical breakpoints.

### 4. Visual Evidence
- Screenshots of the execution terminal.
- Screenshots of the Android Emulator reflecting root status (e.g., `# id`).
- Video recording of the final, end-to-end exploit execution from cold boot.

### 5. Binary Evidence
- Extracted `vmlinux` binary (tracked via hash if too large for repo).
- Generated offset databases (`target_db.kxdb`).
- Raw symbol dumps and `.json` layout extracts.

### 6. Meta-Evidence
- Timeline of milestones.
- SHA256 hashes of all compiled exploit binaries and shellcode payloads.
- Commit logs tracking the evolution of the `libxdk` framework for ARM64.

## Archive Structure
Evidence must be stored hierarchically inside `tier2/artifacts/` or `tier2/logs/` and explicitly referenced in the `03_RESEARCH_LOG.md`.
