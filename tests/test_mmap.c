#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

int main() {
    int fd = open("testfile.bin", O_RDWR | O_CREAT, 0666);
    if (fd < 0) {
        perror("open");
        return 1;
    }
    // Zwiększ plik
    ftruncate(fd, 4096);
    
    char *ptr = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return 1;
    }
    // Modyfikacja pamięci
    for (int i = 0; i < 4096; i++) {
        ptr[i] = 'B';
    }

    // Dodanie alokacji dynamicznej pamięci
    char *dynamic_memory = (char *)malloc(1024 * 1024 * 64); // 64 MB
    if (!dynamic_memory) {
        perror("malloc");
        munmap(ptr, 4096);
        close(fd);
        return 1;
    }
    for (int i = 0; i < 1024 * 1024; i++) {
        dynamic_memory[i] = 'A';
    }
    free(dynamic_memory);

    munmap(ptr, 4096);
    close(fd);
    return 0;
}