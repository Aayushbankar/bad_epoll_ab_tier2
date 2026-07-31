import gdb
import time

class SignalBreakpoint(gdb.Breakpoint):
    def stop(self):
        try:
            flags = int(gdb.parse_and_eval("$x0"))
            print(f"[*] ksys_unshare hit with flags: {flags}")
            if flags == 1111:
                print("[*] Got 1111!")
            elif flags == 2222:
                print("[*] Got 2222!")
        except Exception as e:
            print(f"Error: {e}")
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
