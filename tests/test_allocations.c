#include <stdio.h>
#include <stdlib.h>

int main() {
    printf("Test alokacji i dealokacji\n");
    char *data = (char *)malloc(1024 * 1024); // 1 MB
    if (!data) {
        perror("malloc");
        return 1;
    }
    for (int i = 0; i < 1024 * 1024; i++) {
        data[i] = 'A';
    }
    free(data);
    return 0;
}