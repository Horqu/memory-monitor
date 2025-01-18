#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>
#include <unistd.h>

int main() {
    key_t key = ftok(".", 'x');
    int shmid = shmget(key, 1024, IPC_CREAT | 0666);
    if (shmid < 0) {
        perror("shmget");
        return 1;
    }
    char *data = (char *)shmat(shmid, NULL, 0);
    if (data == (char *)(-1)) {
        perror("shmat");
        return 1;
    }
    strcpy(data, "Hello from shared memory!");
    printf("%s\n", data);
    shmdt(data);
    shmctl(shmid, IPC_RMID, NULL);
    return 0;
}