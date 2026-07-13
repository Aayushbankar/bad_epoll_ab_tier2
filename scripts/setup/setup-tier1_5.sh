#!/bin/bash
# ============================================================================
# Tier 1.5 Setup Script — KernelCTF Baseline Environment
# ============================================================================
# This script automates the environment setup for Tier 1.5 of the bad-epoll-lab.
# It creates a clean workspace, downloads the exact pre-compiled bzImage from 
# the KernelCTF release bucket, clones an unmodified copy of the exploit PoC, 
# creates a rootfs, and packages the exploit for QEMU execution.
#
# Usage: ./setup-tier1_5.sh
# Run from: /mnt/work/company/cyphermatrix/repos/bad-epoll-lab/
# ============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LAB_DIR="$(dirname "$SCRIPT_DIR")"
TIER1_5_DIR="$LAB_DIR/exploit/tier1_5-kernelctf-env"
KERNEL_VERSION="lts-6.12.67"
BZIMAGE_URL="https://storage.googleapis.com/kernelctf-build/releases/${KERNEL_VERSION}/bzImage"
POC_REPO="https://github.com/J-jaeyoung/security-research.git"
POC_BRANCH="submit-cve-2026-46242"

echo "============================================"
echo "  bad-epoll-lab — Tier 1.5 Setup (Baseline)"
echo "  Target Kernel: KernelCTF ${KERNEL_VERSION}"
echo "============================================"
echo ""

# Step 1: Create clean directory
echo "[1/5] Creating Tier 1.5 workspace..."
mkdir -p "$TIER1_5_DIR"
cd "$TIER1_5_DIR"
echo "  ✓ Workspace created at $TIER1_5_DIR"

# Step 2: Download exact KernelCTF bzImage
echo ""
echo "[2/5] Downloading pre-compiled KernelCTF bzImage..."
if [ -f "bzImage" ]; then
    echo "  ✓ bzImage already exists, skipping download."
else
    wget -q --show-progress "$BZIMAGE_URL" -O bzImage
    echo "  ✓ bzImage downloaded."
fi

# Step 3: Clone unmodified exploit PoC
echo ""
echo "[3/5] Cloning clean, unmodified exploit PoC..."
if [ -d "security-research" ]; then
    echo "  ✓ Exploit source already cloned, skipping."
else
    git clone -b "$POC_BRANCH" --depth 1 "$POC_REPO"
    echo "  ✓ Exploit source cloned."
fi

# Step 4: Compile the exploit
echo ""
echo "[4/5] Compiling the original exploit..."
EXPLOIT_DIR="security-research/pocs/linux/kernelctf/CVE-2026-46242_lts_cos/exploit/${KERNEL_VERSION}"

if [ -f "$EXPLOIT_DIR/exploit" ]; then
    echo "  ✓ Exploit already compiled, skipping."
    cp "$EXPLOIT_DIR/exploit" .
else
    if [ ! -d "$EXPLOIT_DIR" ]; then
        echo "  ✗ Error: Exploit source directory not found at $EXPLOIT_DIR."
        exit 1
    fi
    echo "  Running make in $EXPLOIT_DIR..."
    pushd "$EXPLOIT_DIR"
    make
    popd
    cp "$EXPLOIT_DIR/exploit" .
    echo "  ✓ Exploit compiled successfully."
fi

# Step 5: Create rootfs and inject exploit
echo ""
echo "[5/5] Creating minimal rootfs and injecting exploit..."
if [ -f "initramfs.cpio" ]; then
    echo "  ✓ initramfs already exists, skipping."
else
    mkdir -p rootfs/{bin,dev,etc,proc,sys,tmp}
    
    # Find busybox
    BUSYBOX=$(command -v busybox 2>/dev/null || echo "")
    if [ -z "$BUSYBOX" ]; then
        echo "  ✗ busybox not found on host. Please install it."
        exit 1
    fi
    cp "$BUSYBOX" rootfs/bin/busybox
    
    # Create symlinks for common commands
    for cmd in sh ls cat echo id whoami mount mkdir chmod cp mv rm dmesg uname; do
        ln -sf busybox "rootfs/bin/$cmd"
    done
    
    # Create init script
    cat > rootfs/init << 'INITEOF'
#!/bin/sh
mount -t proc none /proc
mount -t sysfs none /sys
mount -t tmpfs none /tmp
mount -t devtmpfs none /dev 2>/dev/null || true

echo ""
echo "==================================================="
echo "  bad-epoll-lab — Tier 1.5 (KernelCTF Baseline)"
echo "  Kernel: $(uname -r)"
echo "  User: $(id)"
echo "==================================================="
echo ""
echo "Original exploit is located at: /bin/exploit"
echo "To run: /bin/exploit"
echo ""

exec /bin/sh
INITEOF
    chmod +x rootfs/init
    
    # Inject exploit
    cp exploit rootfs/bin/exploit
    chmod +x rootfs/bin/exploit
    
    # Package initramfs
    cd rootfs
    find . -print0 | cpio --null -ov --format=newc > ../initramfs.cpio 2>/dev/null
    cd ..
    echo "  ✓ rootfs created and exploit injected: initramfs.cpio"
fi

echo ""
echo "============================================"
echo "  Tier 1.5 Setup Complete!"
echo "============================================"
echo ""
echo "To boot the KernelCTF environment and test the unmodified exploit, run:"
echo ""
echo "qemu-system-x86_64 \\"
echo "  -kernel $TIER1_5_DIR/bzImage \\"
echo "  -initrd $TIER1_5_DIR/initramfs.cpio \\"
echo "  -append 'console=ttyS0 quiet kaslr' \\"
echo "  -nographic -smp 2 -m 2G -cpu kvm64,+smep,+smap"
echo ""
echo "Note: If the exploit crashes due to KASLR bypass (rdtscp) SIGILL on kvm64,"
echo "you may need to append 'nokaslr' to the append string, or boot with '-cpu host'."
