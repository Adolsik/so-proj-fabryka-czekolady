#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include "common/shared.h"

struct sembuf sem_P = {0, -1, 0}; // Czekaj na semafor 0 (Muteks)
struct sembuf sem_V = {0, 1, 0};  // Zwolnij semafor 0 (Muteks)

void acquire_mutex(int semid) {
    if (semop(semid, &sem_P, 1) == -1) {
        perror("semop P error");
        // Obsługa błędu...
    }
}

void release_mutex(int semid) {
    if (semop(semid, &sem_V, 1) == -1) {
        perror("semop V error");
        // Obsługa błędu...
    }
}

void supplier_process(enum Component component_type, int supplier_id) {
    // Dostęp do IPC
    int shmid = shmget(KEY_SHM, sizeof(WarehouseState), 0); // 0 bo już istnieje
    // ... obsługa błędu ...
    WarehouseState *shm_ptr = (WarehouseState *)shmat(shmid, NULL, 0);
    // ... obsługa błędu ...
    int semid = semget(KEY_SEM, 1, 0); // 0 bo już istnieje
    // ... obsługa błędu ...

    int size_to_add = component_size[component_type];

    while (1) {
        sleep(rand() % 3 + 1); // Czekaj 1-3 sekundy

        acquire_mutex(semid);

        if (shm_ptr->occupied_units + size_to_add <= shm_ptr->capacity_N) {
            // Dodawanie składnika
            shm_ptr->occupied_units += size_to_add;
            shm_ptr->count[component_type]++;

            printf("Dostawca %d (Składnik %c): Dostarczył. Magazyn: %d/%d\n", 
                   supplier_id, 'A' + component_type, 
                   shm_ptr->occupied_units, shm_ptr->capacity_N);
            // Zapis do pliku raportu TODO
        } else {
            printf("Dostawca %d (Składnik %c): Magazyn pełny. Czekam...\n", 
                   supplier_id, 'A' + component_type);
            // Zapis do pliku raportu TODO
        }

        release_mutex(semid);
        
        // Tutaj sprawdzenie, czy Dyrektor wysłał polecenie_3 (przez Kolejkę Komunikatów)
    }

    // ... sprzątanie shmdt ...
}