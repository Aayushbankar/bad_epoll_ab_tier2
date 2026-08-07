# Scripts
Automation scripts for booting, building, and running the emulator.

## Evidence-log hygiene

Harness gdb scripts write raw evidence logs via `set logging file evidence/<NAME>.log`
(relative to the `tier2/` directory; run scripts invoke gdb from `tier2/`).

**Never re-run a harness over a log that is already committed.** Re-running
clobbers the committed raw evidence with `set logging overwrite on` / `rm -f`.
If an experiment must be re-run:

1. Move the committed log aside first, e.g.
   `git mv evidence/EXP-018_raw_gdb.log evidence/EXP-018_raw_gdb_run_<date>.log`,
   or copy the current file to a run-suffixed name and `git checkout --` the
   committed one.
2. Add a provenance header to the preserved variant explaining what it is
   (see `EXP-018_raw_gdb_run_20260805_failed.log` for the pattern).

Do not hardcode absolute repo paths in `set logging file`; use the relative
`evidence/...` form so runs are portable and the clobber scope is obvious.
