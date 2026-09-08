#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <string.h>

#define KEY_SPEC_PROCESS_KEYRING -2

int main() {
    char payload[4096];
    memset(payload, 'A', sizeof(payload));
    long ret = syscall(__NR_add_key, "user", "test", payload, sizeof(payload), KEY_SPEC_PROCESS_KEYRING);
    if (ret < 0) {
        perror("add_key");
        return 1;
    }
    printf("add_key success: %ld\n", ret);
    return 0;
}
