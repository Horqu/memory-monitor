#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

int main() {
    // Test 1: Basic mmap and munmap
    int fd = open("testfile.bin", O_RDWR | O_CREAT, 0666);
    if (fd < 0) {
        perror("open");
        return 1;
    }
    // Zwiększ rozmiar pliku
    if (ftruncate(fd, 1024 * 8) == -1) {
        perror("ftruncate");
        close(fd);
        return 1;
    }

    char *ptr = mmap(NULL, 1024 * 8, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return 1;
    }
    // Modyfikacja pamięci
    for (int i = 0; i < 1024 * 8; i++) {
        ptr[i] = 'B';
    }

    // Test 2: Alokacja dynamiczna pamięci
    char *dynamic_memory = (char *)malloc(1024 * 1024); // 1 MB
    if (!dynamic_memory) {
        perror("malloc");
        munmap(ptr, 1024 * 8);
        close(fd);
        return 1;
    }
    for (int i = 0; i < 1024 * 1024; i++) {
        dynamic_memory[i] = 'A';
    }
    // free(dynamic_memory);

    // Test 3: Realokacja pamięci do większego rozmiaru
    dynamic_memory = (char *)realloc(dynamic_memory, 2 * 1024 * 1024); // 2 MB
    if (!dynamic_memory) {
        perror("realloc");
        munmap(ptr, 1024 * 8);
        close(fd);
        return 1;
    }
    memset(dynamic_memory, 'C', 2 * 1024 * 1024);
    free(dynamic_memory);

    // Test 4: Realokacja pamięci do mniejszego rozmiaru
    dynamic_memory = (char *)malloc(512 * 1024); // 512 KB
    if (!dynamic_memory) {
        perror("malloc");
        munmap(ptr, 1024 * 8);
        close(fd);
        return 1;
    }
    memset(dynamic_memory, 'D', 512 * 1024);
    free(dynamic_memory);

    // Test 5: mmap z zerową długością (powinno się nie powieść)
    ptr = mmap(NULL, 0, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr != MAP_FAILED) {
        fprintf(stderr, "mmap z zerową długością powiodło się nieoczekiwanie\n");
        munmap(ptr, 0);
        close(fd);
        return 1;
    } else {
        perror("mmap z zerową długością");
    }

    int fd_read = open("testfile.bin", O_RDONLY);
    if (fd_read < 0) {
        perror("open read-only");
        munmap(ptr, 1024 * 8);
        close(fd);
        return 1;
    }

    // Test 6: mmap z nieprawidłowymi uprawnieniami (tylko do odczytu)
    ptr = mmap(NULL, 1024, PROT_WRITE, MAP_SHARED, fd_read, 0);
    if (ptr == MAP_FAILED) {
        perror("mmap z nieprawidłowymi uprawnieniami");
    } else {
        fprintf(stderr, "mmap z nieprawidłowymi uprawnieniami powiodło się nieoczekiwanie\n");
        munmap(ptr, 1024);
        close(fd);
        return 1;
    }

    // Test 7: Multiple mmap allocations
    char *ptr1 = mmap(NULL, 512, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr1 == MAP_FAILED) {
        perror("mmap ptr1");
    }

    char *ptr2 = mmap(NULL, 2048, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr2 == MAP_FAILED) {
        perror("mmap ptr2");
    }

    // Modyfikacja pamięci w ptr1 i ptr2
    if (ptr1 != MAP_FAILED) {
        memset(ptr1, 'E', 512);
        munmap(ptr1, 512);
    }
    if (ptr2 != MAP_FAILED) {
        memset(ptr2, 'F', 2048);
        munmap(ptr2, 2048);
    }

    // Test 8: Repeated mmap and munmap
    for (int i = 0; i < 10; i++) {
        char *temp_ptr = mmap(NULL, 1024, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (temp_ptr == MAP_FAILED) {
            perror("mmap w pętli");
            break;
        }
        memset(temp_ptr, 'G', 1024);
        munmap(temp_ptr, 1024);
    }

    // Test 9: Zwalnianie nieprawidłowego wskaźnika (powinno się nie powieść)
    void *invalid_ptr = (void *)0xDEADBEEF;
    if (munmap(invalid_ptr, 1024) == -1) {
        perror("munmap nieprawidłowego wskaźnika");
    } else {
        fprintf(stderr, "munmap nieprawidłowego wskaźnika powiodło się nieoczekiwanie\n");
        close(fd);
        return 1;
    }

    printf("Wszystkie testy mmap zakończone pomyślnie.\n");
    return 0;
}