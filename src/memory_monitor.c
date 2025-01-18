#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <pthread.h>

// Deklaracje wskaźników do prawdziwych funkcji
static void *(*real_malloc)(size_t) = NULL;
static void (*real_free)(void *) = NULL;
static void *(*real_calloc)(size_t, size_t) = NULL;
static void *(*real_realloc)(void *, size_t) = NULL;
static void *(*real_mmap)(void *, size_t, int, int, int, off_t) = NULL;
static void *(*real_mmap64)(void *, size_t, int, int, int, off_t) = NULL;
static int   (*real_munmap)(void *, size_t) = NULL;
static int   (*real_munmap64)(void *, size_t) = NULL;
static void *(*real_sbrk)(intptr_t) = NULL;

// Funkcje pomocnicze
static void printUsage();
static void safe_log(const char *format, ...);

// Globalne zmienne do śledzenia mmap
static size_t total_mmap_alloc = 0;
static size_t total_mmap_dealloc = 0;

// Struktura do śledzenia alokacji
typedef struct Allocation {
    void *ptr;
    size_t size;
    struct Allocation *next;
} Allocation;

static Allocation *allocations_head = NULL;
static pthread_mutex_t alloc_lock = PTHREAD_MUTEX_INITIALIZER;
static size_t total_malloc_alloc = 0;

// Funkcja bezpiecznego logowania
static void safe_log(const char *format, ...) {
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    va_list args;

    pthread_mutex_lock(&lock);
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    pthread_mutex_unlock(&lock);
}

// Dodaj alokację do listy
static void add_allocation(void *ptr, size_t size) {
    Allocation *new_alloc = real_malloc(sizeof(Allocation));
    if (!new_alloc) return;
    new_alloc->ptr = ptr;
    new_alloc->size = size;
    pthread_mutex_lock(&alloc_lock);
    new_alloc->next = allocations_head;
    allocations_head = new_alloc;
    total_malloc_alloc += size;
    pthread_mutex_unlock(&alloc_lock);
}

// Usuń alokację z listy
static void remove_allocation(void *ptr) {
    pthread_mutex_lock(&alloc_lock);
    Allocation *current = allocations_head;
    Allocation *prev = NULL;
    while (current) {
        if (current->ptr == ptr) {
            if (prev) {
                prev->next = current->next;
            } else {
                allocations_head = current->next;
            }
            total_malloc_alloc -= current->size;
            real_free(current);
            break;
        }
        prev = current;
        current = current->next;
    }
    pthread_mutex_unlock(&alloc_lock);
}

__attribute__((constructor))
static void init_library() {
    safe_log("Initializing memory_monitor library.\n");
    real_malloc   = dlsym(RTLD_NEXT, "malloc");
    real_free     = dlsym(RTLD_NEXT, "free");
    real_calloc   = dlsym(RTLD_NEXT, "calloc");
    real_realloc  = dlsym(RTLD_NEXT, "realloc");
    real_mmap     = dlsym(RTLD_NEXT, "mmap");
    real_mmap64   = dlsym(RTLD_NEXT, "mmap64");
    real_munmap   = dlsym(RTLD_NEXT, "munmap");
    real_munmap64 = dlsym(RTLD_NEXT, "munmap64");
    real_sbrk     = dlsym(RTLD_NEXT, "sbrk");

    safe_log("Initialized memory_monitor library.\n");
    printUsage();
}

__attribute__((destructor))
static void fini_library() {
    printUsage();
    safe_log("Final state - malloc_alloc=%zu bytes | mmap_alloc=%zu bytes | total_alloc=%zu bytes\n",
             total_malloc_alloc, total_mmap_alloc - total_mmap_dealloc, total_malloc_alloc + (total_mmap_alloc - total_mmap_dealloc));
}

// Funkcja pomocnicza do wypisywania w KB/MB/stronach
static void printUsage() {
    size_t current_mmap_alloc = total_mmap_alloc - total_mmap_dealloc;
    size_t total_alloc = total_malloc_alloc + current_mmap_alloc;
    int page_size = getpagesize();
    safe_log("[usage] malloc_alloc=%zu bytes | ~%zu KB | ~%.2f MB | ~%zu pages | mmap_alloc=%zu bytes | total_alloc=%zu bytes\n",
             total_malloc_alloc,
             total_malloc_alloc / 1024,
             (double)total_malloc_alloc / (1024.0 * 1024.0),
             total_alloc / page_size,
             current_mmap_alloc,
             total_alloc
    );
}

void *malloc(size_t size) {
    void *ptr = real_malloc(size);
    if (ptr) {
        add_allocation(ptr, size);
        safe_log("[malloc] size=%zu | ptr=%p\n", size, ptr);
        printUsage();
    }
    return ptr;
}

void free(void *ptr) {
    if (ptr) {
        remove_allocation(ptr);
        safe_log("[free] ptr=%p\n", ptr);
        printUsage();
    }
    real_free(ptr);
}

void *calloc(size_t nmemb, size_t size) {
    void *ptr = real_calloc(nmemb, size);
    if (ptr) {
        add_allocation(ptr, nmemb * size);
        safe_log("[calloc] nmemb=%zu size=%zu | ptr=%p\n", nmemb, size, ptr);
        printUsage();
    }
    return ptr;
}

void *realloc(void *ptr, size_t size) {
    if (ptr) {
        remove_allocation(ptr);
    }
    void *new_ptr = real_realloc(ptr, size);
    if (new_ptr) {
        add_allocation(new_ptr, size);
        safe_log("[realloc] ptr=%p new_size=%zu | new_ptr=%p\n", ptr, size, new_ptr);
        printUsage();
    }
    return new_ptr;
}

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    void *res = real_mmap(addr, length, prot, flags, fd, offset);
    if (res != MAP_FAILED) {
        pthread_mutex_lock(&alloc_lock);
        total_mmap_alloc += length;
        pthread_mutex_unlock(&alloc_lock);
        safe_log("[mmap] length=%zu fd=%d offset=%ld | res=%p\n", length, fd, offset, res);
        printUsage();
    }
    return res;
}

void *mmap64(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    void *res = real_mmap64(addr, length, prot, flags, fd, offset);
    if (res != MAP_FAILED) {
        pthread_mutex_lock(&alloc_lock);
        total_mmap_alloc += length;
        pthread_mutex_unlock(&alloc_lock);
        safe_log("[mmap64] length=%zu fd=%d offset=%ld | res=%p\n", length, fd, offset, res);
        printUsage();
    }
    return res;
}

int munmap(void *addr, size_t length) {
    int ret = real_munmap(addr, length);
    if (ret == 0) {
        pthread_mutex_lock(&alloc_lock);
        total_mmap_dealloc += length;
        pthread_mutex_unlock(&alloc_lock);
        safe_log("[munmap] length=%zu | addr=%p\n", length, addr);
        printUsage();
    }
    return ret;
}

int munmap64(void *addr, size_t length) {
    int ret = real_munmap64(addr, length);
    if (ret == 0) {
        pthread_mutex_lock(&alloc_lock);
        total_mmap_dealloc += length;
        pthread_mutex_unlock(&alloc_lock);
        safe_log("[munmap64] length=%zu | addr=%p\n", length, addr);
        printUsage();
    }
    return ret;
}

void *sbrk(intptr_t increment) {
    void *res = real_sbrk(increment);
    safe_log("[sbrk] increment=%ld | new_brk=%p\n", (long)increment, res);
    printUsage();
    return res;
}