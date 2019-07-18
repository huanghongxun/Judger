#include <stdio.h>
#include <time.h>

int main() {
    srand(time(NULL));
    printf("%d", rand() % 1000000);
    return 0;
}
