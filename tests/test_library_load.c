#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

typedef void (*hello_func)();

int main() {
    // Ładowanie biblioteki libhello.so
    void *handle = dlopen("src/libhello.so", RTLD_LAZY);
    if (!handle) {
        fprintf(stderr, "Failed to load libhello.so: %s\n", dlerror());
        return EXIT_FAILURE;
    }

    // Resetowanie błędów dlerror
    dlerror();

    // Pobieranie symbolu 'hello'
    hello_func hello = (hello_func)dlsym(handle, "hello");
    char *error = dlerror();
    if (error != NULL)  {
        fprintf(stderr, "Failed to find symbol 'hello': %s\n", error);
        dlclose(handle);
        return EXIT_FAILURE;
    }

    // Wywołanie funkcji 'hello'
    hello();

    // Zamknięcie biblioteki
    if (dlclose(handle) != 0) {
        fprintf(stderr, "Failed to close libhello.so: %s\n", dlerror());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}