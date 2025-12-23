#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include "shared.h"

#define STATE_FILE "magazyn.dat"

// Struktura do przechowywania PIDów, by wiedzieć kogo zamykać
typedef struct {
    pid_t suppliers[4];
    pid_t workers[2];
} FactoryPIDs;

void cleanup(int shmid, int semid, int msqid) {
    shmctl(shmid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID);
    msgctl(msqid, IPC_RMID, NULL);
    printf("\n[Dyrektor] Zasoby IPC usunięte z systemu.\n");
}

void save_state(WarehouseState *state) {
    FILE *f = fopen(STATE_FILE, "wb");
    if (f) {
        fwrite(state, sizeof(WarehouseState), 1, f);
        fclose(f);
        printf("[Dyrektor] Stan magazynu zapisany do pliku.\n");
    }
}

void load_state(WarehouseState *state) {
    FILE *f = fopen(STATE_FILE, "rb");
    if (f) {
        fread(state, sizeof(WarehouseState), 1, f);
        fclose(f);
        printf("[Dyrektor] Stan magazynu odtworzony z pliku.\n");
    }
}

int main() {
    int N;
    printf("Podaj pojemność magazynu N: ");
    if (scanf("%d", &N) != 1) exit(1);

    // 1. Inicjalizacja IPC (Shared Memory, Semaphores)
    int shmid = shmget(KEY_SHM, sizeof(WarehouseState), IPC_CREAT | 0600);
    WarehouseState *shm_ptr = (WarehouseState *)shmat(shmid, NULL, 0);
    
    // Inicjalizacja stanu
    shm_ptr->capacity_N = N;
    shm_ptr->occupied_units = 0;
    for(int i=0; i<4; i++) shm_ptr->count[i] = 0;
    load_state(shm_ptr); // Odtwórz jeśli plik istnieje

    int semid = semget(KEY_SEM, 1, IPC_CREAT | 0600);
    semctl(semid, 0, SETVAL, 1);

    int msqid = msgget(KEY_MSG, IPC_CREAT | 0600);

    FactoryPIDs factory;

    // 2. Uruchamianie 4 Dostawców
    for (int i = 0; i < 4; i++) {
        pid_t p = fork();
        if (p == 0) {
            char type[2], name[20];
            sprintf(type, "%d", i);
            sprintf(name, "Dostawca_%c", 'A' + i);
            execl("./supplier", "./supplier", type, name, NULL);
            exit(1);
        } else {
            factory.suppliers[i] = p;
        }
    }

    // 3. Uruchamianie 2 Pracowników
    for (int i = 0; i < 2; i++) {
        pid_t p = fork();
        if (p == 0) {
            char type[2];
            sprintf(type, "%d", i + 1);
            execl("./worker", "./worker", type, NULL);
            exit(1);
        } else {
            factory.workers[i] = p;
        }
    }

    

    // 4. Menu Dyrektora (Sygnały Linuxowe)
    int cmd = 0;
    while (cmd != 4) {
        printf("\nPOLECENIA DYREKTORA:\n1: Stop Pracownicy\n2: Stop Magazyn\n3: Stop Dostawcy\n4: Stop Wszystko i Zapisz\nWybor: ");
        scanf("%d", &cmd);

        switch(cmd) {
            case 1: // Stop Fabryka (Pracownicy)
                for(int i=0; i<2; i++) kill(factory.workers[i], SIGUSR1);
                break;
            case 3: // Stop Dostawcy
                for(int i=0; i<4; i++) kill(factory.suppliers[i], SIGUSR1);
                break;
            case 4: // Stop Wszystko
                for(int i=0; i<4; i++) kill(factory.suppliers[i], SIGTERM);
                for(int i=0; i<2; i++) kill(factory.workers[i], SIGTERM);
                save_state(shm_ptr);
                break;
        }
    }

    // Czekanie na zakończenie wszystkich dzieci
    while(wait(NULL) > 0);

    cleanup(shmid, semid, msqid);
    return 0;
}