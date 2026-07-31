#!/bin/bash
killall qemu-system-aarch64
cd /mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2
DEBUG=1 ./scripts/run_qemu.sh > /tmp/qemu_run.log 2>&1 &
sleep 2
gdb -q -batch -x scripts/gdb_exp011_timerfd_pivot.py android/artifacts/vmlinux > evidence/EXP-011_raw_gdb.log 2>&1
