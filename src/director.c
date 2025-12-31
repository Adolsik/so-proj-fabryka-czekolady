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
#include <errno.h>

#define STATE_FILE "magazyn.dat"

// Struktura do przechowywania PIDów, by wiedzieć kogo zamykać
typedef struct {
    pid_t suppliers[4];
    pid_t workers[2];
} FactoryPIDs;

// Funkcja czyszcząca zasoby IPC
void cleanup(int shmid, int semid, int msqid);

// Funkcja zamykająca dostęp do magazynu
void close_warehouse_access(int semid);
   
// Zapisywanie stanu magazynu
void save_state(WarehouseState *state);

// Wczytywanie stanu magazynu
void load_state(WarehouseState *state);
  

int main() {
    int N;

    // Inicjalizacja IPC (Shared Memory, Semaphores)
    int shmid = shmget(KEY_SHM, sizeof(WarehouseState), IPC_CREAT | 0600);
        if (shmid == -1) {
        perror("[Dyrektor] shmget error");
        exit(EXIT_FAILURE);
    }
    WarehouseState *shm_ptr = (WarehouseState *)shmat(shmid, NULL, 0);
        if (shm_ptr == (void *)-1) {
        perror("[Dyrektor] shmat error");
        exit(EXIT_FAILURE);
    }
    
    if (access(STATE_FILE, F_OK) == 0) {
        printf("[Dyrektor] Znaleziono zapisany stan magazynu. Wczytuję dane...\n");
        load_state(shm_ptr);
        N = shm_ptr->capacity_N; 
        printf("[Dyrektor] Pojemność odtworzona z pliku: %d\n", N);
    } else {
        printf("[Dyrektor] Brak zapisanego stanu. Podaj pojemność magazynu N: ");
        if (scanf("%d", &N) != 1 || N <= 0) {
            fprintf(stderr, "Błąd: N musi być liczbą dodatnią.\n");
            exit(EXIT_FAILURE);
        }
        
        // Inicjalizacja nowej struktury
        shm_ptr->capacity_N = N;
        shm_ptr->occupied_units = 0;
        for (int i = 0; i < MAX_COMPONENTS; i++) {
            shm_ptr->count[i] = 0;
        }
        printf("[Dyrektor] Zainicjalizowano nowy magazyn o pojemności: %d\n", N);
    }

    shm_ptr->is_open = 1; // Magazyn jest otwarty na start

    int semid = semget(KEY_SEM, 1, IPC_CREAT | 0600);
    if (semid == -1) {
        perror("[Dyrektor] semget error");
        exit(EXIT_FAILURE);
    }

    semctl(semid, 0, SETVAL, 1);
    if (semctl(semid, 0, SETVAL, 1) == -1) {
        perror("[Dyrektor] semctl (SETVAL) error");
        exit(EXIT_FAILURE);
    }

    int msqid = msgget(KEY_MSG, IPC_CREAT | 0600);

    FactoryPIDs factory;

    // Uruchamianie 4 Dostawców
    for (int i = 0; i < 4; i++) {
        pid_t p = fork();
        if (p == -1) {
            perror("[Dyrektor] fork error");
            exit(EXIT_FAILURE);
        }
        else if (p == 0) {
            char type[2], name[20];
            sprintf(type, "%d", i);
            sprintf(name, "Dostawca_%c", 'A' + i);
            execl("./supplier", "./supplier", type, name, NULL);
            if (execl("./supplier", "./supplier", type, name, NULL) == -1) {
                perror("[Dyrektor] execl error");
                exit(EXIT_FAILURE);
            }
            exit(1);
        } else {
            factory.suppliers[i] = p;
        } 
    }

    // Uruchamianie 2 Pracowników
    for (int i = 0; i < 2; i++) {
        pid_t p = fork();
        if (p == -1) {
            perror("[Dyrektor] fork error");
            exit(EXIT_FAILURE);
        }
        else if (p == 0) {
            char type[2];
            sprintf(type, "%d", i + 1);
            execl("./worker", "./worker", type, NULL);
            if (execl("./worker", "./worker", type, NULL) == -1) {
                perror("[Dyrektor] execl error");
                exit(EXIT_FAILURE);
            }
            exit(1);
        } else {
            factory.workers[i] = p;
        }
    }

    // Menu Dyrektora 
    int cmd = 0;
    while (cmd != 5) {
        printf("\nPOLECENIA DYREKTORA:\n1: Stop Pracownicy\n2: Stop Magazyn\n3: Stop Dostawcy\n4: Stop Pracownicy i Magazyn\n5: Wyjście i zapis\nWybor: ");
        scanf("%d", &cmd);

        switch(cmd) {
            case 1: // Stop Fabryka (Pracownicy)
                for(int i=0; i<2; i++) kill(factory.workers[i], SIGUSR1);
                printf("[Dyrektor] Fabryka przestaje pracować.\n");
                log_event("DYREKTOR", "Zatrzymano pracę fabryki (Polecenie 1)");
                break;
            case 2: // Stop Magazyn
                shm_ptr->is_open = 0;
                semctl(semid, 0, SETVAL, 0);
                printf("[Dyrektor] Magazyn został zablokowany. Procesy czekają...\n");
                log_event("DYREKTOR", "Zablokowano dostep do magazynu (Polecenie 2)");
                break;
            case 3: // Stop Dostawcy
                for(int i=0; i<4; i++) kill(factory.suppliers[i], SIGUSR2);
                printf("[Dyrektor] Dostawcy przestają dostarczać składniki.\n");
                log_event("DYREKTOR", "Zatrzymano dostawców (Polecenie 3)");
                break;
            case 4: // Stop Fabryka, Magazyn
                for(int i=0; i<2; i++) kill(factory.workers[i], SIGUSR1);
                shm_ptr->is_open = 0;
                semctl(semid, 0, SETVAL, 0);
                printf("[Dyrektor] Fabryka i Magazyn przestaje pracować.\n");
                log_event("DYREKTOR", "Zablokowano pracę fabryki i dostęp do magazynu (Polecenie 4)");
                break;
            case 5: // Wyjście
                for(int i=0; i<2; i++) kill(factory.workers[i], SIGUSR1);
                for(int i=0; i<4; i++) kill(factory.suppliers[i], SIGUSR2);
                sleep(1); // Czekaj na zakończenie procesów
                save_state(shm_ptr);
                log_event("DYREKTOR", "Zapisano stan magazynu i zakończono działanie programu");
                break;
        }
    }

    // Czekanie na zakończenie wszystkich dzieci i sprzątanie (zombie itp)
    while(wait(NULL) > 0);

    cleanup(shmid, semid, msqid);
    return 0;
}

void cleanup(int shmid, int semid, int msqid) {
    shmctl(shmid, IPC_RMID, NULL);
    if (shmctl(shmid, IPC_RMID, NULL) == -1 && errno != EINVAL) perror("cleanup: shmctl error");
    semctl(semid, 0, IPC_RMID);
    if (semctl(semid, 0, IPC_RMID) == -1 && errno != EINVAL) perror("cleanup: semctl error");
    msgctl(msqid, IPC_RMID, NULL);
    if (msgctl(msqid, IPC_RMID, NULL) == -1 && errno != EINVAL) perror("cleanup: msgctl error");
    printf("\n[Dyrektor] Zasoby IPC usunięte z systemu.\n");
}

void save_state(WarehouseState *state) {
    FILE *f = fopen(STATE_FILE, "wb");
    if (f == NULL) {
        perror("[Dyrektor] fopen (save_state) error");
    }
    if (f) {
        fwrite(state, sizeof(WarehouseState), 1, f);
        fclose(f);
        printf("[Dyrektor] Stan magazynu zapisany do pliku.\n");
    }
}

void load_state(WarehouseState *state) {
    FILE *f = fopen(STATE_FILE, "rb");
    if (f == NULL) {
        perror("[Dyrektor] fopen (load_state) error");
    }
    if (f) {
        fread(state, sizeof(WarehouseState), 1, f);
        fclose(f);
        printf("[Dyrektor] Stan magazynu odtworzony z pliku.\n");
    }
}