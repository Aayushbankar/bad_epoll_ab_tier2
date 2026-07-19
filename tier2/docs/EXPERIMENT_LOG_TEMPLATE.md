# Experiment Log Template

**Date/Time:** [YYYY-MM-DDTHH:MM:SSZ]
**Host Kernel:** [e.g., Linux 6.1.1-arch1-1 x86_64]
**Target Kernel:** [e.g., Android ARM64 6.1.23]
**Git Commit/Hash:** [e.g., 7e35917775b8...]

## Execution Configuration
- **Exact Command:** [The exact bash command used to start the experiment]
- **Runtime Configuration:** [Any specific environment variables, binary injected]
- **QEMU Parameters:** [e.g., CPU, RAM, Network settings]
- **KASAN Configuration:** [e.g., kasan=off or kasan=on]
- **Number of Iterations:** [Number of loops or times the trigger was executed]

## Results
### Standard Output (stdout)
```
[Paste relevant stdout here or 'None']
```

### Standard Error (stderr)
```
[Paste relevant stderr here or 'None']
```

### Kernel Logs (dmesg)
```
[Paste relevant dmesg output, panic logs, or KASAN reports here]
```

## Analysis
- **Crash/OOPS/KASAN Result:** [Did the kernel panic? Was a KASAN report generated?]
- **Reproducible?:** [Yes / No / Flaky (X% success rate)]
- **Conservative Interpretation:** [What does this evidence actually prove? Avoid claiming exploit success unless there is definitive proof. E.g., 'The program executed without error, but no KASAN UAF report was generated. This does not prove the vulnerability was triggered.']
