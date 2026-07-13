#!/usr/bin/expect -f
set timeout 600
spawn nc localhost 1234
# No wait for qemu monitor, connect to serial console instead
