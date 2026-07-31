import gdb
import time
import sys

class SignalBreakpoint(gdb.Breakpoint):
    def stop(self):
        try:
            val = gdb.parse_and_eval("$x0")
            print(f"[*] SignalBreakpoint hit! $x0 = {val}", file=sys.stderr)
            sys.stderr.flush()
            flags = int(val)
            if flags == 1111:
                print(f"[*] Got 1111", file=sys.stderr)
                sys.stderr.flush()
        except Exception as e:
            print(f"[*] EXCEPTION: {e}", file=sys.stderr)
            sys.stderr.flush()
        return False

def setup():
    gdb.execute("set pagination off")
    gdb.execute("set confirm off")
    gdb.execute("set non-stop off")
    
    retries = 30
    while retries > 0:
        try:
            gdb.execute("target remote :1234")
            break
        except gdb.error:
            time.sleep(1)
            retries -= 1
            
    SignalBreakpoint("ksys_unshare")
    gdb.execute("continue")

setup()
