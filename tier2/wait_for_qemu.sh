#!/bin/bash
tail -f evidence/EXP-029/EXP-029_raw_serial.log | while read line; do
    if echo "$line" | grep -q "GENUINE ROOT PRIVILEGE ESCALATION"; then
        echo "SUCCESS DETECTED"
        pkill -f qemu-system-aarch64
        exit 0
    elif echo "$line" | grep -q "Failed to achieve root"; then
        echo "FAILED DETECTED"
        pkill -f qemu-system-aarch64
        exit 0
    elif echo "$line" | grep -q "Kernel panic"; then
        echo "PANIC DETECTED"
        pkill -f qemu-system-aarch64
        exit 0
    elif echo "$line" | grep -q "Unable to handle kernel"; then
        echo "PANIC DETECTED"
        pkill -f qemu-system-aarch64
        exit 0
    fi
done
