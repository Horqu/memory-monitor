#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <unistd.h>

// deklaracje wskaźników do prawdziwych funkcji
static void *(*real_malloc)(size_t) = NULL;
static void (*real_free)(void *) = NULL;
static void *(*real_calloc)(size_t, size_t) = NULL;
static void *(*real_realloc)(void *, size_t) = NULL;
static void *(*real_mmap)(void *, size_t, int, int, int, off_t) = NULL;
static int   (*real_munmap)(void *, size_t) = NULL;
static void *(*real_sbrk)(intptr_t) = NULL;

__attribute__((constructor))
static void init_library() {
    real_malloc  = dlsym(RTLD_NEXT, "malloc");
    real_free    = dlsym(RTLD_NEXT, "free");
    real_calloc  = dlsym(RTLD_NEXT, "calloc");
    real_realloc = dlsym(RTLD_NEXT, "realloc");
    real_mmap    = dlsym(RTLD_NEXT, "mmap");
    real_munmap  = dlsym(RTLD_NEXT, "munmap");
    real_sbrk    = dlsym(RTLD_NEXT, "sbrk");
}

// funkcje pomocnicze do wypisywania w KB/MB/stronach
static void printUsage() {
    struct mallinfo mi = mallinfo();
    int page_size = getpagesize();
    fprintf(stderr,
        "[usage] uordblks=%d bytes | ~%d KB | ~%.2f MB | ~%d pages\n",
        mi.uordblks,
        mi.uordblks / 1024,
        (double)mi.uordblks / (1024.0 * 1024.0),
        mi.uordblks / page_size
    );
}

void *malloc(size_t size) {
    void *ptr = real_malloc(size);
    struct mallinfo mi = mallinfo();
    fprintf(stderr, "[malloc] size=%zu | arena=%d | ordblks=%d | uordblks=%d\n",
            size, mi.arena, mi.ordblks, mi.uordblks);
    printUsage();
    return ptr;
}

void free(void *ptr) {
    real_free(ptr);
    struct mallinfo mi = mallinfo();
    fprintf(stderr, "[free] arena=%d | ordblks=%d | uordblks=%d\n",
            mi.arena, mi.ordblks, mi.uordblks);
    printUsage();
}

void printCurrentMallinfo() {
    struct mallinfo mi = mallinfo();
    fprintf(stderr, "[mallinfo] arena=%d | ordblks=%d | uordblks=%d\n",
            mi.arena, mi.ordblks, mi.uordblks);
}

void setMallopt(int param, int value) {
    mallopt(param, value);
}

// haczenie calloc
void *calloc(size_t nmemb, size_t size) {
    void *ptr = real_calloc(nmemb, size);
    fprintf(stderr, "[calloc] nmemb=%zu size=%zu\n", nmemb, size);
    printUsage();
    return ptr;
}

// haczenie realloc
void *realloc(void *ptr, size_t size) {
    void *new_ptr = real_realloc(ptr, size);
    fprintf(stderr, "[realloc] size=%zu\n", size);
    printUsage();
    return new_ptr;
}

// haczenie mmap
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    void *res = real_mmap(addr, length, prot, flags, fd, offset);
    fprintf(stderr, "[mmap] length=%zu fd=%d offset=%ld\n", length, fd, offset);
    printUsage();
    return res;
}

// haczenie munmap
int munmap(void *addr, size_t length) {
    int ret = real_munmap(addr, length);
    fprintf(stderr, "[munmap] length=%zu\n", length);
    printUsage();
    return ret;
}

// obserwacja sbrk (brk)
void *sbrk(intptr_t increment) {
    void *res = real_sbrk(increment);
    fprintf(stderr, "[sbrk] increment=%ld\n", (long)increment);
    printUsage();
    return res;
}