# Project Recovery Guide

This document is the ultimate failsafe for the `bad-epoll-lab` repository. If the entire environment, QEMU image, compiled exploit, and dependencies are accidentally deleted, following this guide will restore the repository to its exact research state as of July 2026.

## 1. Reproducibility Checklist
Before beginning the recovery, ensure the host machine meets these requirements:
- [ ] **Host OS:** Fedora 40+ or Ubuntu 24.04+
- [ ] **Disk Space:** 20GB free space
- [ ] **Internet Access:** Required to fetch kernel sources and GitHub repositories

## 2. Full State Recovery Procedure

### Step 1: Clone the Repository
```bash
git clone https://github.com/cyphermatrix/bad-epoll-lab.git
cd bad-epoll-lab
```

### Step 2: Install Required Dependencies
*These dependencies cover both the kernel compilation and the exploit's C++ toolchain requirements.*

**Fedora:**
```bash
sudo dnf install -y gcc gcc-c++ make qemu-system-x86-core busybox kernel-devel openssl-devel elfutils-libelf-devel flex bison cpio cmake keyutils-libs-devel libstdc++-static glibc-static patch
```
**Ubuntu:**
```bash
sudo apt update
sudo apt install -y build-essential qemu-system-x86 qemu-utils busybox-static cpio libncurses-dev bison flex libssl-dev libelf-dev cmake libkeyutils-dev patch
```

### Step 3: Run the Automated Setup
```bash
./scripts/setup-tier1.sh
```
This script will download Linux 6.12.67, configure the kernel, compile the `bzImage`, create the `initramfs`, and clone the exploit PoC.

### Step 4: Apply Manual Exploit Patches
Because the Google kernelCTF exploit is tailored for GCC 11/12 and their proprietary kernel layout, it will fail to compile on GCC 16 and crash on our custom `vmlinux`. Apply the following commands to fully patch the environment.

**4a. Fix C++ Standard Template Errors in `libxdk`:**
```bash
cd exploit/tier1-linux-vm/security-research/pocs/linux/kernelctf/CVE-2026-46242_lts_cos/exploit/lts-6.12.67/kernel-research/libxdk
sed -i '1i #include <functional>' payloads/PayloadBuilder.cpp
sed -i 's/std::reference_wrapper<PayloadData>/PayloadData*/g' payloads/PayloadBuilder.cpp
cd ..
```

**4b. Patch the Exploit Target and KASLR Leak:**
```bash
# Bypass target auto-detection and hardcode our LTS target
sed -i 's/kxdb.AutoDetectTarget()/kxdb.GetTarget("kernelctf", "lts-6.12.67")/g' exploit.cpp

# Remove timing-based KASLR leak which causes SIGILL on kvm64 QEMU profiles
sed -i 's/leak_kaslr()/kernel_base = 0xffffffff81000000; \/\/ leak_kaslr()/g' exploit.cpp
```

**4c. Apply Custom `task_struct` Offsets:**
```bash
# Update offsets to match the locally compiled Fedora 6.12.67 kernel
sed -i 's/comm = 1928/comm = 1840/g' exploit.cpp
sed -i 's/files = 1984/files = 1896/g' exploit.cpp
sed -i 's/children = 1472/children = 1384/g' exploit.cpp
sed -i 's/sibling = 1488/sibling = 1400/g' exploit.cpp
```

### Step 5: Compile the Exploit
```bash
make
```

### Step 6: Inject and Boot
```bash
cd ../../../../../../
# Copy exploit to rootfs
cp security-research/pocs/linux/kernelctf/CVE-2026-46242_lts_cos/exploit/lts-6.12.67/exploit rootfs/bin/exploit
chmod +x rootfs/bin/exploit

# Package the initramfs
cd rootfs
find . -print0 | cpio --null -ov --format=newc > ../initramfs.cpio
cd ..

# Boot the VM
qemu-system-x86_64 -kernel linux-6.12.67/arch/x86/boot/bzImage -initrd initramfs.cpio -append 'console=ttyS0 quiet nokaslr' -nographic -smp 2 -m 2G -cpu kvm64,+smep,+smap
```

You are now back at the exact Tier 1 research state.

## 3. Missing Documentation Check
The repository originally lacked explicit `.patch` files or command-line scripts to automate the exploit modifications. By including the `sed` commands directly in this Recovery Guide, the missing link between a fresh clone and a working exploit has been completely resolved.
