# Experiment Protocol

This document establishes the permanent rulebook for every experiment executed within this repository. Adherence is non-negotiable.

## RULE 1: Evidence lives in the repo, not in the agent environment.
Every experiment run must copy its full raw output (GDB output, dmesg, QEMU log) into `tier2/evidence/<experiment-id>/` as a real committed file BEFORE any doc references it. A doc may only cite a path under `tier2/evidence/`. Citing any path outside the repo (session directories, `/home/*/.gemini/`, `/tmp/`, task logs, brain folders, or any other agent-internal storage) is absolutely forbidden.

## RULE 2: No result is "done" until the raw evidence file has been opened and read in full.
Before writing any status word (`SUCCESS`, `VERIFIED`, `CONFIRMED`, or similar), the acting agent must have actually printed/cat'd/read the complete raw evidence file in the active session and quoted the specific lines that support the claim. A claim with no quoted raw output attached is not permitted to use those words — it must be labeled `UNVERIFIED` or `IN PROGRESS` instead.

## RULE 3: Timeouts, hangs, and crashes are never silently reinterpreted as success.
If a script exits via timeout, connection error, or unexpected termination, the experiment result is `FAILED`. It is never partially credited, nor justified by "the data still shows X." Investigate why separately, but the ledger entry for that run is `FAILED`.

## RULE 4: Every experiment gets a unique ID, logged the moment it starts, not after it succeeds.
Append to `EXPERIMENT_INDEX.md` when an experiment BEGINS, with status `RUNNING`. Update to `PASSED`/`FAILED`/`INCONCLUSIVE` only after Rule 2 is satisfied. This prevents failed attempts from disappearing from the historical record.

## RULE 5: No hardcoded addresses without a derivation record.
Any hex address, offset, or symbol location used in a script must either:
(a) Be resolved at GDB runtime via symbol name, not hardcoded.
(b) If hardcoded for performance, be accompanied by a comment stating the exact `vmlinux` build (git commit + build timestamp) it was derived from, and the script must assert that the currently-running `vmlinux` matches before proceeding, aborting loudly if it doesn't.

## RULE 6: Static analysis and runtime observation are never labeled the same way.
Any claim based on reading source code, disassembly, or struct layout without executing anything is tagged `STATIC`. Any claim based on an actual traced execution is tagged `RUNTIME`. Never let a `STATIC` claim be visually or verbally indistinguishable from a `RUNTIME` one in any document.

## RULE 7: One canonical status file.
`PROJECT_STATE.md` is the only file allowed to state overall project status. Every other doc (progress reports, mentor updates, notebooks) must link to it rather than restating status independently.

## RULE 8: Every retracted claim stays visible.
Never delete a ledger entry once assigned an ID. Mark it `RETRACTED` with a one-line reason and a link to the correction. The failure record is permanent and part of the project's value.

## RULE 9: Corrections to the vulnerability model are broadcast, not buried.
If the understanding of the bug's mechanics changes, the entire `docs/` and `05_research/` tree must be searched for the old (incorrect) claim, and every instance must be fixed or flagged in the same session.
