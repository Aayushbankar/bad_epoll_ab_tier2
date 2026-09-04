#include <unistd.h>
#include <sys/syscall.h>

void __attribute__((noinline)) my_print(const char* msg, int len) {
    syscall(SYS_write, 1, msg, len);
}

int main() {
    my_print("hello\n", 6);
    return 0;
}
