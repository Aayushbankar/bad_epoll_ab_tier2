# Reproducing the Exploit in Tier 1.5

This document details how to set up, launch, and run the KernelCTF baseline environment (Tier 1.5) using the provided setup scripts on the `main` branch. 

Tier 1.5 uses the pre-compiled `bzImage` from KernelCTF and the unmodified exploit source to establish a verified baseline.

## 1. Initial Setup
To build the Tier 1.5 environment, you must run the automated setup script from the root of the repository.

Navigate to the repository root and execute:
```bash
./scripts/setup-tier1_5.sh
```

**What this script does:**
1. Downloads the exact pre-compiled KernelCTF `bzImage` (LTS 6.12.67).
2. Clones the exact, unmodified exploit PoC from the original author's repository.
3. Compiles the exploit locally.
4. Generates a minimal `rootfs` with `busybox`.
5. Injects the compiled exploit into `rootfs/bin/exploit` and packages it into `initramfs.cpio`.

## 2. Directory Context
After running the setup script, navigate to the Tier 1.5 environment directory:
```bash
cd exploit/tier1_5-kernelctf-env
```

You should see the following critical files:
- `bzImage`: The vulnerable kernel image downloaded directly from KernelCTF.
- `initramfs.cpio`: The root filesystem containing the compiled exploit.
- `exploit`: The compiled exploit binary (also inside the initramfs).
- `security-research/`: The cloned repository containing the unmodified PoC source code.

## 3. Launching the VM
To boot the VM and interact with the console, use the following `qemu-system-x86_64` command directly from the `tier1_5-kernelctf-env` directory:

```bash
qemu-system-x86_64 \
  -kernel bzImage \
  -initrd initramfs.cpio \
  -append 'console=ttyS0 quiet kaslr' \
  -nographic -smp 2 -m 2G -cpu kvm64,+smep,+smap
```

*Note: If the exploit crashes due to the KASLR bypass relying on `rdtscp` (which causes a SIGILL on the default `kvm64` CPU), you may append `nokaslr` to the `-append` string, or change the CPU flag to `-cpu host` (if KVM is available).*

Wait for the boot sequence to complete. You will be dropped into a root shell (`~ #`).

## 4. Running the Exploit
Inside the QEMU root shell, simply execute:
```bash
/bin/exploit
```

To exit the VM, use the QEMU monitor sequence `Ctrl+A` then `X`, or kill the QEMU process from another terminal.

## 5. Debugging Constraints
Unlike Tier 1, **Tier 1.5 does not natively provide a `vmlinux` file** because the kernel is downloaded as a pre-compiled `bzImage`. 

If you attempt to attach GDB using the `vmlinux` compiled in Tier 1 (`exploit/tier1-linux-vm/linux-6.12.67/vmlinux`), your symbols and offsets will be completely mismatched because Tier 1.5 uses a Clang-compiled kernel from Google, while Tier 1 is compiled with GCC locally. 

To debug Tier 1.5 with GDB, you must first extract the `vmlinux` from the downloaded KernelCTF `bzImage` (e.g., using `extract-vmlinux`) and append `-s` to the QEMU boot command.
