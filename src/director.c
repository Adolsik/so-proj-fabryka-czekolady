#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <unistd.h>
#include "common/shared.h"

// Do semaforów użyjemy unii dla starszych systemów, ale nowoczesne POSIX jej nie wymagają.
// Dla System V IPC (shmget, semget, msgget) jest to standardowe podejście:
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
    struct seminfo *__buf;
};

int main(int argc, char *argv[]) {

    int N = 100; // Domyślna pojemność magazynu

    // Tworzenie Pamięci Dzielonej
    int shmid = shmget(KEY_SHM, sizeof(WarehouseState), IPC_CREAT | 0600);
    if (shmid == -1) {
        perror("Wystapil blad przy tworzeniu pamieci dzielonej");
        exit(EXIT_FAILURE);
    }
    WarehouseState *shm_ptr = (WarehouseState *)shmat(shmid, NULL, 0);
    if (shm_ptr == (void *)-1) {
        perror("Wystapil blad przy dolaczaniu pamieci dzielonej");
        exit(EXIT_FAILURE);
    }

    // Inicjalizacja stanu magazynu. Na poczatku jest pusty. Później dane bedą aktualizowane z pliku.
    shm_ptr->capacity_N = N;
    shm_ptr->occupied_units = 0;
    for (int i = 0; i < MAX_COMPONENTS; i++) {
        shm_ptr->count[i] = 0;
    }

    // Tworzenie Zestawu Semaforów
    // Semafor 0: Muteks (blokada dostępu do pamięci dzielonej)
    // Semafor 1-4: Warunki (np. "czy jest miejsce w magazynie", "czy jest składnik A")
    // Użyjmy tylko jednego semafora (indeks 0) jako Muteks:
    int semid = semget(KEY_SEM, 1, 0600 | IPC_CREAT);
    if (semid == -1) {
        perror("Wystapil blad przy tworzeniu semafora");
        exit(EXIT_FAILURE);
    }

    // Inicjalizacja muteksa na 1 (wolny)
    union semun arg;
    arg.val = 1;
    if (semctl(semid, 0, SETVAL, arg) == -1) {
        perror("Wystapil blad przy inicjalizacji semafora");
        exit(EXIT_FAILURE);
    }

    // Tworzenie Kolejki Komunikatów
    int msqid = msgget(KEY_MSG, 0600 | IPC_CREAT);
    if (msqid == -1) {
        perror("Wystapil blad przy tworzeniu kolejki komunikatow");
        exit(EXIT_FAILURE);
    }
    
    printf("Dyrektor: IPC inicjalizowane (shmid: %d, semid: %d, msqid: %d)\n", shmid, semid, msqid);

    pid_t pid = fork();

    if (pid == -1) {
        perror("fork error");
        // ... obsługa błędu ...
    } else if (pid == 0) {
        // Proces potomny (np. Dostawca A)
        // Parametry: (ID procesu, typ komponentu, klucz SHM, klucz SEM, klucz MSG)
        char *args[] = {"./supplier", "0", "0", NULL}; // Dostawca A, indeks 0
        if (execv(args[0], args) == -1) {
            perror("execv supplier error");
            exit(EXIT_FAILURE);
        }
    } else {
        // Proces macierzysty (Dyrektor) kontynuuje
    }

    //  Uruchomienie Procesów (Dostawcy, Magazyn, Pracownicy) TODO
    
    //  Wysyłanie Poleceń TODO

    // Sprzątanie TODO

    return 0;
}

// Funkcja pomocnicza do operacji na semaforach (P - czekaj, V - zwolnij)
// Używana przez WSZYSTKIE procesy do ochrony dostępu do Pamięci Dzielonej
struct sembuf sem_P = {0, -1, 0}; // Czekaj na semafor 0 (Muteks)
struct sembuf sem_V = {0, 1, 0};  // Zwolnij semafor 0 (Muteks)

void acquire_mutex(int semid) {
    if (semop(semid, &sem_P, 1) == -1) {
        perror("semop P error");
        // Obsługa błędu
    }
}

void release_mutex(int semid) {
    if (semop(semid, &sem_V, 1) == -1) {
        perror("semop V error");
        // Obsługa błędu
    }
}