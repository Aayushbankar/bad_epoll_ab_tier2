#!/bin/bash
# ============================================================================
# Tier 1 Setup Script — Linux VM via QEMU
# ============================================================================
# This script automates the environment setup for Tier 1 of the bad-epoll-lab.
# It downloads the vulnerable kernel, compiles it, creates a rootfs, clones
# the exploit, and prepares everything for QEMU boot.
#
# Usage: ./setup-tier1.sh
# Run from: /mnt/work/company/cyphermatrix/repos/bad-epoll-lab/
# ============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LAB_DIR="$(dirname "$SCRIPT_DIR")"
TIER1_DIR="$LAB_DIR/exploit/tier1-linux-vm"
KERNEL_VERSION="6.12.67"
KERNEL_URL="https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-${KERNEL_VERSION}.tar.xz"

echo "============================================"
echo "  bad-epoll-lab — Tier 1 Setup"
echo "  Target Kernel: Linux ${KERNEL_VERSION} LTS"
echo "============================================"
echo ""

# Step 1: Check and install dependencies
echo "[1/7] Checking build dependencies..."
DEPS_NEEDED=""

check_dep() {
    if ! command -v "$1" &>/dev/null; then
        DEPS_NEEDED="$DEPS_NEEDED $2"
    fi
}

check_dep gcc gcc
check_dep g++ gcc-c++
check_dep make make
check_dep qemu-system-x86_64 qemu-system-x86
check_dep cpio cpio
check_dep flex flex
check_dep bison bison

if [ -n "$DEPS_NEEDED" ]; then
    echo "  Missing packages:$DEPS_NEEDED"
    echo "  Please install them with your package manager."
    echo "  Fedora: sudo dnf install -y$DEPS_NEEDED qemu-system-x86-core busybox kernel-devel openssl-devel elfutils-libelf-devel"
    echo "  Ubuntu: sudo apt install -y build-essential qemu-system-x86 qemu-utils busybox-static cpio libncurses-dev bison flex libssl-dev libelf-dev"
    echo ""
    echo "  After installing, re-run this script."
    exit 1
fi
echo "  ✓ All build dependencies found."

# Step 2: Download kernel source
echo ""
echo "[2/7] Downloading Linux kernel ${KERNEL_VERSION}..."
cd "$TIER1_DIR"
if [ -d "linux-${KERNEL_VERSION}" ]; then
    echo "  ✓ Kernel source already exists, skipping download."
else
    wget -q --show-progress "$KERNEL_URL"
    tar -xf "linux-${KERNEL_VERSION}.tar.xz"
    rm -f "linux-${KERNEL_VERSION}.tar.xz"
    echo "  ✓ Kernel source extracted."
fi

# Step 3: Configure kernel
echo ""
echo "[3/7] Configuring kernel for QEMU..."
cd "linux-${KERNEL_VERSION}"
if [ -f ".config" ]; then
    echo "  ✓ Kernel already configured, skipping."
else
    make defconfig
    scripts/config -e CONFIG_EPOLL
    scripts/config -e CONFIG_DEBUG_INFO
    scripts/config -e CONFIG_DEBUG_INFO_DWARF5
    scripts/config -e CONFIG_TIMERFD
    # Keep KASLR enabled (the exploit handles it)
    make olddefconfig
    echo "  ✓ Kernel configured."
fi

# Step 4: Compile kernel
echo ""
echo "[4/7] Compiling kernel (this may take 15-30 minutes)..."
if [ -f "arch/x86/boot/bzImage" ]; then
    echo "  ✓ Kernel already compiled, skipping."
else
    make -j"$(nproc)" 2>&1 | tail -5
    echo "  ✓ Kernel compiled: arch/x86/boot/bzImage"
fi
cd "$TIER1_DIR"

# Step 5: Create rootfs
echo ""
echo "[5/7] Creating minimal rootfs..."
if [ -f "initramfs.cpio" ]; then
    echo "  ✓ initramfs already exists, skipping."
else
    mkdir -p rootfs/{bin,dev,etc,proc,sys,tmp}
    
    # Find busybox
    BUSYBOX=$(command -v busybox 2>/dev/null || echo "")
    if [ -z "$BUSYBOX" ]; then
        echo "  ✗ busybox not found. Install it:"
        echo "    Fedora: sudo dnf install busybox"
        echo "    Ubuntu: sudo apt install busybox-static"
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
echo "============================================"
echo "  bad-epoll-lab — Tier 1 VM"
echo "  Kernel: $(uname -r)"
echo "  User: $(id)"
echo "============================================"
echo ""
echo "Exploit is at /bin/exploit (if injected)"
echo "Run: /bin/exploit"
echo ""

exec /bin/sh
INITEOF
    chmod +x rootfs/init
    
    # Package initramfs
    cd rootfs
    find . -print0 | cpio --null -ov --format=newc > ../initramfs.cpio 2>/dev/null
    cd "$TIER1_DIR"
    echo "  ✓ rootfs created: initramfs.cpio"
fi

# Step 6: Clone exploit
echo ""
echo "[6/7] Cloning exploit source..."
if [ -d "security-research" ]; then
    echo "  ✓ Exploit source already cloned, skipping."
else
    git clone -b submit-cve-2026-46242 --depth 1 \
        https://github.com/J-jaeyoung/security-research.git
    echo "  ✓ Exploit source cloned."
fi

# Step 7: Summary
echo ""
echo "============================================"
echo "  Setup Complete!"
echo "============================================"
echo ""
echo "Next steps:"
echo "  1. cd $TIER1_DIR"
echo "  2. Review the exploit source:"
echo "     less security-research/pocs/linux/kernelctf/CVE-2026-46242_lts_cos/exploit/lts-6.12.67/exploit.cpp"
echo "  3. Compile the exploit:"
echo "     g++ -static -O2 -o exploit exploit.cpp -lpthread"
echo "     (Note: may need libxdk — check the source for dependencies)"
echo "  4. Inject into rootfs:"
echo "     cp exploit rootfs/tmp/ && cd rootfs && find . -print0 | cpio --null -ov --format=newc > ../initramfs.cpio"
echo "  5. Boot QEMU:"
echo "     qemu-system-x86_64 -kernel linux-${KERNEL_VERSION}/arch/x86/boot/bzImage -initrd initramfs.cpio -append 'console=ttyS0 quiet' -nographic -smp 2 -m 2G -cpu kvm64,+smep,+smap"
echo ""
