#include <unistd.h>
#include <sys/syscall.h>
int main() {
    syscall(SYS_getpid);
    syscall(SYS_getuid);
    return 0;
}
