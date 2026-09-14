#include <stdio.h>
#include <string.h>
int main() {
    FILE *f = fopen("/proc/kallsyms", "r");
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "ep_eventpoll_poll\n")) {
            printf("KALLSYM: %s", line);
            break;
        }
    }
    fclose(f);
    return 0;
}
