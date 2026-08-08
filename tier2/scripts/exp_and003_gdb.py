import gdb
import time

gdb.execute("set pagination off")
gdb.execute("set confirm off")

# Enable GDB logging directly to the evidence file
gdb.execute("set logging file evidence/AND-003_raw_enforcing.log")
gdb.execute("set logging overwrite on")
gdb.execute("set logging enabled on")

def log(msg):
    print(f"[*] {msg}", flush=True)

def connect():
    for i in range(15):
        try:
            gdb.execute("target remote :1234")
            log("Connected to QEMU via GDB :1234")
            return True
        except gdb.error:
            time.sleep(1)
    return False

if not connect():
    log("Failed to connect to QEMU")
    gdb.execute("quit")

class WriteTrace(gdb.Breakpoint):
    def __init__(self):
        super(WriteTrace, self).__init__("ksys_write", internal=False)

    def stop(self):
        fd = int(gdb.parse_and_eval("$x0"))
        if fd == 1 or fd == 2:
            buf = int(gdb.parse_and_eval("$x1"))
            count = int(gdb.parse_and_eval("$x2"))
            try:
                data = gdb.selected_inferior().read_memory(buf, count).tobytes()
                text = data.decode("utf-8", errors="ignore")
                if "AND-003" in text or "TEST" in text:
                    log(f"HARNESS MSG: {text.strip()}")
            except Exception as e:
                pass
        return False

class RebootTrace(gdb.Breakpoint):
    def __init__(self):
        super(RebootTrace, self).__init__("__arm64_sys_reboot", internal=False)

    def stop(self):
        log("RUNTIME TRACE: __arm64_sys_reboot hit! Harness completed successfully.")
        gdb.execute("detach")
        gdb.execute("quit")
        return False

WriteTrace()
RebootTrace()

log("Breakpoints set on ksys_write and __arm64_sys_reboot. Continuing execution...")
gdb.execute("continue")
