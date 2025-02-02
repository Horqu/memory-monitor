/**
 * @file memory_monitor.c
 * @brief A library that intercepts memory allocation function calls (malloc, free, mmap, etc.).
 *
 * This file contains the implementation of a library that intercepts calls
 * to memory allocation and deallocation functions. It tracks and reports
 * memory usage during program execution. It also includes the definitions
 * of init_library() and fini_library(), which are executed upon loading
 * and unloading the library.
 */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <pthread.h>

/**
 * @brief Pointer to the original malloc function from the standard library.
 */
static void *(*real_malloc)(size_t) = NULL;
/**
 * @brief Pointer to the original free function.
 */
static void (*real_free)(void *) = NULL;
/**
 * @brief Pointer to the original calloc function.
 */
static void *(*real_calloc)(size_t, size_t) = NULL;
/**
 * @brief Pointer to the original realloc function.
 */
static void *(*real_realloc)(void *, size_t) = NULL;
/**
 * @brief Pointer to the original mmap function.
 */
static void *(*real_mmap)(void *, size_t, int, int, int, off_t) = NULL;
/**
 * @brief Pointer to the original mmap64 function.
 */
static void *(*real_mmap64)(void *, size_t, int, int, int, off_t) = NULL;
/**
 * @brief Pointer to the original munmap function.
 */
static int   (*real_munmap)(void *, size_t) = NULL;
/**
 * @brief Pointer to the original munmap64 function.
 */
static int   (*real_munmap64)(void *, size_t) = NULL;
/**
 * @brief Pointer to the original sbrk function.
 */
static void *(*real_sbrk)(intptr_t) = NULL;
/**
 * @brief Pointer to the original dlopen function.
 */
static void *(*real_dlopen)(const char *, int) = NULL;
/**
 * @brief Pointer to the original dlclose function.
 */
static int   (*real_dlclose)(void *) = NULL;

/**
 * @brief Utility function to print memory usage.
 *
 * This function displays information about the current amount
 * of memory allocated by malloc/calloc/realloc and mmap.
 */
static void printUsage();

/**
 * @brief Thread-safe logging function.
 *
 * @param format Format string (printf-style).
 * @param ... Additional arguments.
 */
static void safe_log(const char *format, ...);

/**
 * @brief Global variable tracking the total amount of memory allocated via mmap.
 */
static size_t total_mmap_alloc = 0;
/**
 * @brief Global variable tracking the total amount of memory deallocated via munmap.
 */
static size_t total_mmap_dealloc = 0;

/**
 * @struct Allocation
 * @brief Structure for tracking memory allocations from malloc/calloc/realloc.
 *
 * Stores a pointer to the allocated memory block, its size,
 * and a pointer to the next element in the singly-linked list.
 */
typedef struct Allocation {
    void *ptr;                  /**< Pointer to the allocated memory block. */
    size_t size;                /**< Size of the allocated memory block. */
    struct Allocation *next;    /**< Pointer to the next list element. */
} Allocation;
 
/**
 * @brief Head of the Allocation list that stores all current allocations from malloc/calloc/realloc.
 */
static Allocation *allocations_head = NULL;

/**
 * @brief Mutex that protects the Allocation list.
 */
static pthread_mutex_t alloc_lock = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief Global variable tracking the total amount of memory allocated by malloc/calloc/realloc.
 */
static size_t total_malloc_alloc = 0;

/**
 * @brief Thread-safe logging function to stderr.
 *
 * Logs events in the library (allocations, deallocations, etc.) in a thread-safe manner.
 *
 * @param format Format string (printf-style).
 * @param ... Additional arguments.
 */
static void safe_log(const char *format, ...) {
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    va_list args;

    pthread_mutex_lock(&lock);
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    pthread_mutex_unlock(&lock);
}

/**
 * @brief Adds a new allocation entry to the Allocation list.
 *
 * @param ptr Pointer returned by the memory allocation function (malloc/calloc/realloc).
 * @param size The size of the allocated memory in bytes.
 */
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

/**
 * @brief Removes an allocation entry from the list when the memory is freed.
 *
 * @param ptr Pointer to the memory block that is being freed.
 */
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

/**
 * @brief Library initialization function (automatically called upon loading).
 *
 * Initializes pointers to the original implementations of functions
 * (malloc, free, mmap, etc.) and logs the initial state of the library.
 */
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
    real_dlopen   = dlsym(RTLD_NEXT, "dlopen");
    real_dlclose   = dlsym(RTLD_NEXT, "dlclose");

    safe_log("Initialized memory_monitor library.\n");
    printUsage();
}

/**
 * @brief Library finalization function (automatically called upon unloading).
 *
 * Logs the final memory usage state before the library is unloaded.
 */
__attribute__((destructor))
static void fini_library() {
    printUsage();
    safe_log("Final state - malloc_alloc=%zu bytes | mmap_alloc=%zu bytes | total_alloc=%zu bytes\n",
             total_malloc_alloc, total_mmap_alloc - total_mmap_dealloc, total_malloc_alloc + (total_mmap_alloc - total_mmap_dealloc));
}

/**
 * @brief Utility function to report memory usage in KB, MB, and page counts.
 *
 * Logs the current usage of memory allocated by malloc/calloc/realloc and mmap.
 */
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

/**
 * @brief Intercepts calls to malloc in order to monitor memory allocation.
 *
 * @param size The number of bytes to allocate.
 * @return A pointer to the allocated memory, or NULL on failure.
 */
void *malloc(size_t size) {
    void *ptr = real_malloc(size);
    if (ptr) {
        add_allocation(ptr, size);
        safe_log("[malloc] size=%zu | ptr=%p\n", size, ptr);
        printUsage();
    }
    return ptr;
}

/**
 * @brief Intercepts calls to free in order to monitor memory deallocation.
 *
 * @param ptr Pointer to the memory block to free.
 */
void free(void *ptr) {
    if (ptr) {
        remove_allocation(ptr);
        safe_log("[free] ptr=%p\n", ptr);
        printUsage();
    }
    real_free(ptr);
}

/**
 * @brief Intercepts calls to calloc in order to monitor memory allocation.
 *
 * @param nmemb Number of elements.
 * @param size Size of each element in bytes.
 * @return A pointer to the allocated memory, or NULL on failure.
 */
void *calloc(size_t nmemb, size_t size) {
    void *ptr = real_calloc(nmemb, size);
    if (ptr) {
        add_allocation(ptr, nmemb * size);
        safe_log("[calloc] nmemb=%zu size=%zu | ptr=%p\n", nmemb, size, ptr);
        printUsage();
    }
    return ptr;
}

/**
 * @brief Intercepts calls to realloc in order to monitor memory reallocation.
 *
 * @param ptr Pointer to the currently allocated memory block (may be NULL).
 * @param size The new size of the memory block, in bytes.
 * @return A pointer to the allocated memory, or NULL on failure.
 */
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

/**
 * @brief Intercepts calls to mmap in order to track memory mapping.
 *
 * @param addr Suggested start address (may be NULL).
 * @param length The size of the mapped area.
 * @param prot Memory protection flags.
 * @param flags Mapping flags.
 * @param fd File descriptor associated with the mapping (if applicable).
 * @param offset Offset in the file (if applicable).
 * @return A pointer to the mapped area, or MAP_FAILED on failure.
 */
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

/**
 * @brief Intercepts calls to mmap64 in order to track memory mapping.
 *
 * @param addr Suggested start address (may be NULL).
 * @param length The size of the mapped area.
 * @param prot Memory protection flags.
 * @param flags Mapping flags.
 * @param fd File descriptor associated with the mapping (if applicable).
 * @param offset Offset in the file (if applicable).
 * @return A pointer to the mapped area, or MAP_FAILED on failure.
 */
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

/**
 * @brief Intercepts calls to munmap in order to track memory deallocation.
 *
 * @param addr The address of the memory area to unmap.
 * @param length The size of the area to unmap.
 * @return 0 on success, or -1 on error.
 */
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

/**
 * @brief Intercepts calls to munmap64 in order to track memory deallocation.
 *
 * @param addr The address of the memory area to unmap.
 * @param length The size of the area to unmap.
 * @return 0 on success, or -1 on error.
 */
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

/**
 * @brief Intercepts calls to sbrk to track changes to the data segment.
 *
 * @param increment The size (in bytes) by which to adjust the program break.
 * @return The new program break, or (void*) -1 on failure.
 */
void *sbrk(intptr_t increment) {
    void *res = real_sbrk(increment);
    safe_log("[sbrk] increment=%ld | new_brk=%p\n", (long)increment, res);
    printUsage();
    return res;
}

/**
 * @brief Intercepts calls to dlopen to monitor shared library loading.
 *
 * @param filename The path to the library.
 * @param flag The flags for opening the library.
 * @return A handle to the library, or NULL on failure.
 */
void *dlopen(const char *filename, int flag) {
    void *handle = real_dlopen(filename, flag);
    if (handle) {
        safe_log("[dlopen] filename=%s | flag=%d | handle=%p\n", filename, flag, handle);
    } else {
        safe_log("[dlopen] filename=%s | flag=%d | failed\n", filename, flag);
    }
    return handle;
}

/**
 * @brief Intercepts calls to dlclose to monitor shared library unloading.
 *
 * @param handle The handle of the library to be unloaded.
 * @return 0 on success, or another value on failure.
 */
int dlclose(void *handle) {
    int ret = real_dlclose(handle);
    safe_log("[dlclose] handle=%p | ret=%d\n", handle, ret);
    return ret;
}