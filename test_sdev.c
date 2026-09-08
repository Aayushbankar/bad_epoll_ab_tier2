#include <sys/stat.h>
#include <stdio.h>
#include <sys/sysmacros.h>

int main() {
    struct stat st;
    stat("/dev/null", &st);
    printf("/dev/null: major %d minor %d sdev %lx\n", major(st.st_dev), minor(st.st_dev), st.st_dev);
    return 0;
}
