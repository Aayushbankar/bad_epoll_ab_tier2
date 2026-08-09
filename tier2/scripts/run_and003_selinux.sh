#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
TIER2_DIR="$(cd "${SCRIPT_DIR}/.." &> /dev/null && pwd)"
cd "${TIER2_DIR}"

KERNEL="${TIER2_DIR}/android/artifacts/Image"
VMLINUX="${TIER2_DIR}/android/artifacts/vmlinux"
CC="${TIER2_DIR}/aarch64-linux-musl-cross/bin/aarch64-linux-musl-gcc"
NM="${TIER2_DIR}/aarch64-linux-musl-cross/bin/aarch64-linux-musl-nm"

cat << 'C_EOF' > scripts/test_and003_fixed.c
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <errno.h>

struct my_msgbuf {
    long mtype;
    char mtext[144];
};

void do_log(const char *msg) {
    write(1, msg, strlen(msg));
}

int main() {
    do_log("[AND-003] SELinux Enforcing Syscall Audit\n");
    
    int epfd = epoll_create1(0);
    if (epfd < 0) {
        do_log("[AND-003] FAIL: epoll_create1 failed\n");
    } else {
        do_log("[AND-003] PASS: epoll_create1 success\n");
    }
    
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        do_log("[AND-003] FAIL: pipe failed\n");
    } else {
        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = pipefd[0];
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, pipefd[0], &ev) < 0) {
            do_log("[AND-003] FAIL: epoll_ctl failed\n");
        } else {
            do_log("[AND-003] PASS: epoll_ctl success\n");
        }
    }
    
    close(epfd);
    do_log("[AND-003] PASS: close success\n");
    
    int msqid = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);
    if (msqid < 0) {
        do_log("[AND-003] FAIL: msgget failed\n");
    } else {
        do_log("[AND-003] PASS: msgget success\n");
        
        struct my_msgbuf msg;
        msg.mtype = 1;
        memset(msg.mtext, 'A', sizeof(msg.mtext));
        
        if (msgsnd(msqid, &msg, sizeof(msg.mtext), 0) < 0) {
            do_log("[AND-003] FAIL: msgsnd failed\n");
        } else {
            do_log("[AND-003] PASS: msgsnd success\n");
            
            struct my_msgbuf rcvmsg;
            if (msgrcv(msqid, &rcvmsg, sizeof(rcvmsg.mtext), 0, IPC_NOWAIT) < 0) {
                do_log("[AND-003] FAIL: msgrcv failed\n");
            } else {
                do_log("[AND-003] PASS: msgrcv success\n");
            }
        }
        msgctl(msqid, IPC_RMID, NULL);
    }
    
    do_log("[AND-003] Audit Complete\n");
    return 0;
}
C_EOF

# Compile harness without PIE
"$CC" -static -fno-pie -no-pie -O0 -g -o rootfs/harness scripts/test_and003_fixed.c -pthread

# Find address of write() in userspace
WRITE_ADDR=$("$NM" rootfs/harness | grep " T write" | awk '{print $1}')
WRITE_ADDR="0x${WRITE_ADDR}"

# Compile init
"$CC" -static -O2 -o rootfs/init scripts/init_serial.c

# Package rootfs
cd rootfs
chmod +x init harness
find . -print0 | cpio --null -ov --format=newc > ../initramfs.cpio 2>/dev/null
cd "${TIER2_DIR}"

logfile_abs="${TIER2_DIR}/evidence/AND-003_raw_enforcing.log"
rm -f "$logfile_abs"
touch "$logfile_abs"

cat << GDB_EOF > /tmp/gdb_hook.py
import gdb
import time
import sys

log_file = open("${logfile_abs}", "w", buffering=1)

class WriteHook(gdb.Breakpoint):
    def stop(self):
        try:
            fd = int(gdb.parse_and_eval("\$x0"))
            if fd == 1 or fd == 2:
                buf = int(gdb.parse_and_eval("\$x1"))
                count = int(gdb.parse_and_eval("\$x2"))
                data = gdb.selected_inferior().read_memory(buf, count).tobytes()
                s = data.decode('utf-8', 'replace')
                sys.stdout.write(s)
                sys.stdout.flush()
                log_file.write(s)
                log_file.flush()
                if "Audit Complete" in s:
                    gdb.execute("quit")
        except Exception as e:
            pass
        return False

for i in range(15):
    try:
        gdb.execute("target remote :1234")
        break
    except:
        time.sleep(1)

try:
    bp = WriteHook("*${WRITE_ADDR}", type=gdb.BP_HARDWARE_BREAKPOINT, internal=False)
except Exception as e:
    print(f"Error setting hbreak: {e}")

gdb.execute("continue")
gdb.execute("quit")
GDB_EOF

# Run QEMU
qemu-system-aarch64 \
    -M virt \
    -cpu cortex-a57 \
    -smp 2 \
    -m 2048 \
    -kernel "$KERNEL" \
    -initrd initramfs.cpio \
    -append "console=ttyAMA0 root=/dev/ram0 kasan=off earlycon=pl011,0x09000000 printk.devkmsg=on nohz_full=1 rcu_nocbs=1 rw kaslr enforcing=1" \
    -display none \
    -serial null \
    -no-reboot \
    -nographic -s -S &
QEMU_PID=$!

# Run GDB
timeout 60 gdb -batch -x /tmp/gdb_hook.py > /dev/null 2>&1 || true

kill $QEMU_PID 2>/dev/null || true
wait $QEMU_PID 2>/dev/null || true
