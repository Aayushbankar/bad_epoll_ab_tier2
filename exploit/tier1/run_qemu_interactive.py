#!/usr/bin/env python3
import socket
import time
import os
import signal
import sys

qemu = None
def cleanup(signum, frame):
    if qemu:
        os.system("killall -9 qemu-system-x86_64")
    sys.exit(0)

signal.signal(signal.SIGINT, cleanup)

os.system("./start_qemu.sh")
time.sleep(10)

# Connect to serial port via some other way, wait start_qemu has console=ttyS0 which goes to stdout, but we piped it to background...
