# Migration Log: Repository Separation

## Migration Event: 2026-08-08

**Action:** Separated `bad-epoll-lab` into standalone Tier 2 repository.

### What Happened

| Item | Details |
|------|--------|
| Original repo | `github.com/Aayushbankar/bad-epoll-lab` (monorepo with Tier 1-3) |
| New repo | `github.com/Aayushbankar/bad_epoll_ab_tier2` (Tier 2 standalone) |
| Branch rename | `tier2-android-port` → `main` |
| Deleted branch | `tier1.5-investigation` |
| Removed dirs | `exploit/tier1/`, `exploit/tier1.5/` |
| Removed files | `CVE-2026-46242_Tier1_Final_Writeup.*`, `tier1_terminal_session.log` |
| Commits preserved | 85 (82 original + 3 pushed + cleanup) |

### What Was Preserved

- All `tier2/` content (docs, evidence, scripts, rootfs) — unchanged
- All `exploit/tier2/` and `exploit/tier3/` — unchanged
- All `third_party/linux-6.12.67/` — unchanged
- `docs/` directory — kept as historical Tier 1 archive
- `research/`, `scripts/`, `archive/`, `article/` — kept for reference
- `artifacts/` — kept (Tier 1 frozen binaries, logs, and databases)

### Where Tier 1 Lives Now

The original `bad-epoll-lab` repository remains on GitHub at:
[`github.com/Aayushbankar/bad-epoll-lab`](https://github.com/Aayushbankar/bad-epoll-lab) (main branch)

It contains:
- Complete Tier 1 x86_64 exploit (verified root shell)
- The `tier2-android-port` branch (pre-separation snapshot)
- All original branches and history

### Documentation Impact

| Old Reference | New Value | Scope |
|--------------|-----------|-------|
| Branch `tier2-android-port` | `main` | Updated in AGENTS.md, RUNNER_GUIDE.md, CURRENT_PROGRESS.md |
| Repo name `bad-epoll-lab` | `bad_epoll_ab_tier2` | Updated in AGENTS.md, README.md |
| `exploit/tier1/` paths | Removed (content in original repo) | Annotated in REPOSITORY_STRUCTURE.md, .gitignore |
| `exploit/tier1.5/` paths | Removed (branch deleted) | Annotated where referenced |
| Hardcoded absolute paths | Converted to relative | Updated in tier2/scripts/*.sh |
| `file:///` URIs | Converted to relative | Updated in tier2/docs/ navigation tables |
| Historical docs (`docs/`, `article/`) | Left untouched | Historical records preserved as-is |
