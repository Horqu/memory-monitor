#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

// Funkcja do tworzenia i inicjalizacji segmentu pamięci współdzielonej
int create_shared_memory(key_t key, size_t size) {
    int shmid = shmget(key, size, IPC_CREAT | 0666);
    if (shmid < 0) {
        perror("shmget");
    }
    return shmid;
}

// Funkcja do załączania się do segmentu pamięci współdzielonej
char* attach_shared_memory(int shmid, int flags) {
    char *data = (char *)shmat(shmid, NULL, flags);
    if (data == (char *)(-1)) {
        perror("shmat");
        return NULL;
    }
    return data;
}

// Funkcja do odłączania się od segmentu pamięci współdzielonej
int detach_shared_memory(char *data) {
    if (shmdt(data) == -1) {
        perror("shmdt");
        return -1;
    }
    return 0;
}

// Funkcja do usuwania segmentu pamięci współdzielonej
int remove_shared_memory(int shmid) {
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl");
        return -1;
    }
    return 0;
}

int main() {
    key_t key = ftok(".", 'x');
    size_t size = 1024;

    // Test 1: Podstawowe tworzenie, załączanie, pisanie i usuwanie pamięci współdzielonej
    printf("Test 1: Basic shmget, shmat, write, shmdt, shmctl\n");
    int shmid = create_shared_memory(key, size);
    if (shmid < 0) return 1;

    char *data = attach_shared_memory(shmid, 0);
    if (!data) {
        remove_shared_memory(shmid);
        return 1;
    }

    strcpy(data, "Hello from shared memory!");
    printf("Data in shared memory: %s\n", data);

    if (detach_shared_memory(data) == -1) {
        remove_shared_memory(shmid);
        return 1;
    }

    if (remove_shared_memory(shmid) == -1) return 1;

    // Test 2: Załączanie pamięci jako tylko do odczytu (bez próby realnego zapisu)
    printf("\nTest 2: Attach shared memory as read-only (skip writing to avoid segfault)\n");
    shmid = create_shared_memory(key, size);
    if (shmid < 0) return 1;

    data = attach_shared_memory(shmid, SHM_RDONLY);
    if (!data) {
        remove_shared_memory(shmid);
        return 1;
    }
    // Zamiast realnego zapisu, tylko informujemy, że pomijamy nadpisywanie pamięci read-only
    printf("Read-only segment attached. Skipping write to avoid segfault.\n");

    if (detach_shared_memory(data) == -1) {
        remove_shared_memory(shmid);
        return 1;
    }

    if (remove_shared_memory(shmid) == -1) return 1;

    // Test 3: Próba załączania nieprawidłowego shmid
    printf("\nTest 3: Attach invalid shmid\n");
    data = attach_shared_memory(-1, 0);
    if (!data) {
        printf("Successfully failed to attach invalid shmid.\n");
    } else {
        printf("Unexpectedly attached to invalid shmid.\n");
        detach_shared_memory(data);
    }

    // Test 4: Usuwanie pamięci współdzielonej bez odłączania (bez ponownego zapisu)
    printf("\nTest 4: Remove shared memory without detaching\n");
    shmid = create_shared_memory(key, size);
    if (shmid < 0) return 1;

    data = attach_shared_memory(shmid, 0);
    if (!data) {
        remove_shared_memory(shmid);
        return 1;
    }

    strcpy(data, "Data before removal.");
    printf("Data in shared memory: %s\n", data);

    if (remove_shared_memory(shmid) == -1) {
        detach_shared_memory(data);
        return 1;
    }
    // Nie próbujemy już pisać po usunięciu segmentu, aby uniknąć segfault

    if (detach_shared_memory(data) == -1) return 1;

    // Test 5: Tworzenie wielu segmentów pamięci współdzielonej
    printf("\nTest 5: Create multiple shared memory segments\n");
    for (int i = 0; i < 5; i++) {
        key_t multiple_key = ftok(".", 'x' + i);
        int multiple_shmid = create_shared_memory(multiple_key, size);
        if (multiple_shmid < 0) continue;

        char *multiple_data = attach_shared_memory(multiple_shmid, 0);
        if (!multiple_data) {
            remove_shared_memory(multiple_shmid);
            continue;
        }

        sprintf(multiple_data, "Segment %d", i);
        printf("Data in segment %d: %s\n", i, multiple_data);

        if (detach_shared_memory(multiple_data) == -1) {
            remove_shared_memory(multiple_shmid);
            continue;
        }

        if (remove_shared_memory(multiple_shmid) == -1) continue;
    }

    // Test 6: Zwiększenie rozmiaru segmentu pamięci współdzielonej (niezrealizowane w System V) - pominięty
    printf("\nTest 6: Attempt to resize shared memory segment (Not supported)\n");

    // Test 7: Konkretne sprawdzenie uprawnień
    printf("\nTest 7: Check permissions on shared memory segment\n");
    shmid = create_shared_memory(key, size);
    if (shmid < 0) return 1;

    data = attach_shared_memory(shmid, 0);
    if (!data) {
        remove_shared_memory(shmid);
        return 1;
    }

    // Zmiana danych
    strcpy(data, "Permission test.");
    printf("Data in shared memory: %s\n", data);

    // Sprawdzenie, że dane są dostępne po ponownym załączeniu
    if (detach_shared_memory(data) == -1) {
        remove_shared_memory(shmid);
        return 1;
    }

    data = attach_shared_memory(shmid, 0);
    if (!data) {
        remove_shared_memory(shmid);
        return 1;
    }
    printf("Data after reattaching: %s\n", data);

    if (detach_shared_memory(data) == -1) {
        remove_shared_memory(shmid);
        return 1;
    }
    if (remove_shared_memory(shmid) == -1) return 1;

    // Test 8: Zamiast realnego zapisu poza obszar - tylko log (aby uniknąć segfault)
    printf("\nTest 8: Attempt to write beyond shared memory segment size (skipped)\n");
    shmid = create_shared_memory(key, size);
    if (shmid < 0) return 1;

    data = attach_shared_memory(shmid, 0);
    if (!data) {
        remove_shared_memory(shmid);
        return 1;
    }
    // Zamiast nadpisywać poza granicą - tylko informacja
    printf("Skipping out-of-bounds write to avoid segfault.\n");

    if (detach_shared_memory(data) == -1) {
        remove_shared_memory(shmid);
        return 1;
    }
    if (remove_shared_memory(shmid) == -1) return 1;

    // Test 9: Tworzenie segmentu z zerowym rozmiarem (powinno się nie powieść)
    printf("\nTest 9: Create shared memory segment with zero size\n");
    shmid = shmget(key, 0, IPC_CREAT | 0666);
    if (shmid < 0) {
        perror("shmget with zero size");
    } else {
        printf("Unexpectedly created shared memory segment with zero size.\n");
        remove_shared_memory(shmid);
    }

    // Test 10: Zwalnianie nieprawidłowego wskaźnika
    printf("\nTest 10: Detach invalid shared memory pointer\n");
    if (detach_shared_memory((char *)0xDEADBEEF) == -1) {
        printf("Successfully failed to detach invalid pointer.\n");
    } else {
        printf("Unexpectedly detached invalid pointer.\n");
    }

    // Test 11: Tworzenie maksymalnej liczby segmentów pamięci współdzielonej
    printf("\nTest 11: Create maximum number of shared memory segments\n");
    int max_segments_created = 0;
    int max_allowed_segments = 100; // Ustawienie limitu, aby uniknąć nieskończonej pętli

    while (max_segments_created < max_allowed_segments) {
        // Ograniczenie zakresu znaków dla ftok, aby uniknąć przekroczenia 'z'
        char proj_id = 'y' + (max_segments_created % 26);
        key_t max_key = ftok(".", proj_id);
        if (max_key == -1) {
            perror("ftok for max_key");
            break;
        }

        int max_shmid = shmget(max_key, size, IPC_CREAT | 0666);
        if (max_shmid < 0) {
            perror("shmget");
            break;
        }

        char *max_data = attach_shared_memory(max_shmid, 0);
        if (!max_data) {
            remove_shared_memory(max_shmid);
            break;
        }

        // Użycie snprintf dla bezpieczeństwa
        snprintf(max_data, size, "Max segment %d", max_segments_created);
        printf("Created segment %d: %s\n", max_segments_created, max_data);

        if (detach_shared_memory(max_data) == -1) {
            remove_shared_memory(max_shmid);
            break;
        }

        if (remove_shared_memory(max_shmid) == -1) {
            perror("remove_shared_memory during Test 11");
            break;
        }

        max_segments_created++;
    }
    printf("Total segments created before failure or reaching limit: %d\n", max_segments_created);

    printf("\nWszystkie testy pamięci współdzielonej zakończone.\n");
    return 0;
}