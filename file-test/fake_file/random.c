#include <stdio.h>
#include <time.h>

int main() {
    srand(time(NULL));
    printf("%d", rand());
    return 0;
}
