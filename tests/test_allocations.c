#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    // Test 1: Multiple malloc allocations
    char *data1 = (char *)malloc(16);
    if (!data1) {
        perror("malloc");
        return 1;
    }
    strcpy(data1, "Test malloc 1");
    printf("%s\n", data1);

    char *data2 = (char *)malloc(32);
    if (!data2) {
        perror("malloc");
        free(data1);
        return 1;
    }
    strcpy(data2, "Test malloc 2");
    printf("%s\n", data2);

    // Test 2: calloc allocation
    char *data3 = (char *)calloc(10, sizeof(char));
    if (!data3) {
        perror("calloc");
        free(data1);
        free(data2);
        return 1;
    }
    strcpy(data3, "Test calloc");
    printf("%s\n", data3);

    // Test 3: realloc to expand
    data1 = (char *)realloc(data1, 32);
    if (!data1) {
        perror("realloc");
        free(data2);
        free(data3);
        return 1;
    }
    strcat(data1, " extended");
    printf("%s\n", data1);

    // Test 4: realloc to shrink
    data2 = (char *)realloc(data2, 16);
    if (!data2) {
        perror("realloc");
        free(data1);
        free(data3);
        return 1;
    }
    printf("%s\n", data2);

    // Test 5: realloc with NULL (equivalent to malloc)
    char *data4 = (char *)realloc(NULL, 24);
    if (!data4) {
        perror("realloc");
        free(data1);
        free(data2);
        free(data3);
        return 1;
    }
    strcpy(data4, "Test realloc NULL");
    printf("%s\n", data4);

    // Test 6: realloc to NULL (equivalent to free)
    data3 = (char *)realloc(data3, 0);
    if (data3) {
        perror("realloc");
        free(data1);
        free(data2);
        free(data4);
        return 1;
    }

    // Free remaining allocations
    free(data1);
    free(data2);
    free(data4);

    printf("All allocations and deallocations completed successfully.\n");
    return 0;
}