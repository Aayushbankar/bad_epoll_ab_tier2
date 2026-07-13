# Tier 1 Working State

This document snapshots the exact engineering environment under which the Tier 1 PoC for CVE-2026-46242 reached successful root execution.

## Environment Variables
- **Git Commit Hash:** `895fe9a00fb800161da036a598e8d842def5774d`
- **Current Branch:** `main`
- **Host Kernel Version:** `7.0.13-200.fc44.x86_64`
- **Compiler Version:** `gcc (GCC) 16.1.1 20260515 (Red Hat 16.1.1-2)`
- **Host Distro:** `Fedora Linux 44 (Workstation Edition)`
- **QEMU Version:** `QEMU emulator version 10.2.2 (qemu-10.2.2-1.fc44)`
- **Python Version:** `Python 3.14.6`

## Cryptographic Hashes (SHA-256)
- **Original target_db.kxdb:** `d17e31d6ec77608a9510e3394cdf2741cfe1142806106591807a8ea1cb1ad2ae`
- **Regenerated target_db.kxdb:** `63542e7f06519fb4148ff247d6f140002f269bfd378086ceb3e2b268538418eb`
- **Exploit Binary:** `95cd9f36cc40e342f038200fee97b386876c0f6f0755cbb0dd0d839447fa1276`
- **vmlinux:** `bce598804ff8d3262925a9a7a86e64c0cecce6771caf4aecc3934906c370ef96`
- **bzImage:** `fe13a933a2c0ac52af889baf3155f5ac0f3a2a86e2ae5c1a16f1d0b05ccf275b`
- **.config:** `972b060793eb9c073f1abdcaaf5e3377ec0a31155b9d7de765920dcc37873e1e`
