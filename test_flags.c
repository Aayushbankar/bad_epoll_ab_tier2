#include <stdio.h>
#include <fcntl.h>

int main() {
    printf("flags: 0x%x\n", O_RDWR | O_CLOEXEC);
    return 0;
}
