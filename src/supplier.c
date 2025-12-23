#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include "shared.h"

// Zmienna sygnalizująca koniec pracy
volatile sig_atomic_t keep_running = 1;

// Obsługa sygnałów od Dyrektora
void signal_handler(int sig) {
    keep_running = 0;
}

// Struktury dla operacji na semaforach (zdefiniowane globalnie dla wygody)
struct sembuf lock_storage = {0, -1, 0}; // P: Czekaj/Zablokuj
struct sembuf unlock_storage = {0, 1, 0}; // V: Zwolnij

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Błąd: Brak argumentów (typ i nazwa).\n");
        exit(EXIT_FAILURE);
    }

    int component_type = atoi(argv[1]); // 0=A, 1=B, 2=C, 3=D
    char *name = argv[2];
    int size = component_size[component_type];

    // Rejestracja sygnałów
    signal(SIGUSR1, signal_handler); // Polecenie 3
    signal(SIGTERM, signal_handler); // Polecenie 4

    // Podłączenie do IPC (Pamięć i Semafory)
    int shmid = shmget(KEY_SHM, sizeof(WarehouseState), 0600);
    if (shmid == -1) { perror("Dostawca shmget"); exit(1); }
    WarehouseState *magazyn = (WarehouseState *)shmat(shmid, NULL, 0);

    int semid = semget(KEY_SEM, 1, 0600);
    if (semid == -1) { perror("Dostawca semget"); exit(1); }

    srand(time(NULL) ^ getpid()); // Inicjalizacja losowości dostaw

    printf("[%s] Rozpoczynam pracę. Dostarczam składnik %c (rozmiar %d)\n", 
           name, 'A' + component_type, size);

    while (keep_running) {
        // Różnicowanie czasu dostawy aby uniknąć zakleszczeń i poprawic process starvation
        // Takie ustawienie czasowe skutkuje tym, że żaden ze składników nie odkłada sie w magazynie
        if (component_type < 2) {
            usleep(500000 + (rand() % 1000000)); // Dostawy A, B co 0.5-1.5s
        } else {
            sleep(2 + (rand() % 2));              // Dostawy C, D co 2-3 sekundy
        }
        // --- WEJŚCIE DO SEKCJI KRYTYCZNEJ ---
        if (semop(semid, &lock_storage, 1) == -1) {
            if (errno == EINTR) continue; // Przerwano sygnałem, sprawdź warunek pętli
            perror("semop lock error"); break;
        }

        // Sprawdzenie miejsca w magazynie
        if (magazyn->occupied_units + size <= magazyn->capacity_N) {
            magazyn->occupied_units += size;
            magazyn->count[component_type]++;
            
            printf("[%s] Dostarczono. Magazyn: %d/%d (A:%d B:%d C:%d D:%d)\n",
                   name, magazyn->occupied_units, magazyn->capacity_N,
                   magazyn->count[0], magazyn->count[1], magazyn->count[2], magazyn->count[3]);
        } else {
            printf("[%s] Magazyn pełny! Oczekiwanie...\n", name);
        }

        // --- WYJŚCIE Z SEKCJI KRYTYCZNEJ ---
        semop(semid, &unlock_storage, 1);
    }

    // Odłączenie od pamięci dzielonej przed końcem
    shmdt(magazyn);
    printf("[%s] Kończę pracę i opuszczam magazyn.\n", name);

    return 0;
}