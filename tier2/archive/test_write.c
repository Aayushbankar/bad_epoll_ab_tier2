#include <unistd.h>
#include <string.h>
int main() {
    write(1, "hello\n", 6);
    return 0;
}
