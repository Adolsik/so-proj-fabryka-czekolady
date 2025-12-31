#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include "shared.h"

WarehouseState *magazyn = NULL;

void cleanup_before_exit();

// Zmienna sygnalizująca koniec pracy
volatile sig_atomic_t keep_running = 1;

// Obsługa sygnałów od Dyrektora
void signal_handler(int sig);

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
    signal(SIGUSR2, signal_handler); // Polecenie Stop Dostawca

    // Podłączenie do IPC (Pamięć i Semafory)
    int shmid = shmget(KEY_SHM, sizeof(WarehouseState), 0600);
    check_error(shmid, "[Dostawca] Błąd shmget (dostęp do pamięci)");

    magazyn = (WarehouseState *)shmat(shmid, NULL, 0);
    check_error((int)(intptr_t)magazyn, "[Dostawca] Błąd shmat (dołączenie pamięci)");

    int semid = semget(KEY_SEM, 1, 0600);
    check_error(semid, "[Dostawca] Błąd semget (dostęp do semafora)");

    srand(time(NULL) ^ getpid()); // Inicjalizacja losowości dostaw

    printf("[%s] Rozpoczynam pracę. Dostarczam składnik %c (rozmiar %d)\n", 
           name, 'A' + component_type, size);

    while (keep_running) {
        if (!magazyn->is_open) {
            sleep(1); continue; // Magazyn zamknięty proces czeka
        }
        // Różnicowanie czasu dostawy aby uniknąć zakleszczeń i poprawic process starvation
        // Takie ustawienie czasowe skutkuje tym, że żaden ze składników nie odkłada sie w magazynie
        if (component_type < 2) {
            usleep(750000 + (rand() % 1000000)); // Dostawy A, B co 0.75-1.5s
        } else {
            sleep(2 + (rand() % 2));              // Dostawy C, D co 2-3 sekundy
        }

        magazyn->supplier_status[component_type] = 2; 

        // --- WEJŚCIE DO SEKCJI KRYTYCZNEJ ---
        if (semop(semid, &lock_storage, 1) == -1) {
            if (errno == EINTR) continue; // Przerwano sygnałem, sprawdź warunek pętli
            perror("[Dostawca] semop lock error"); break;
        }

        // Sprawdzenie miejsca w magazynie
        if (magazyn->occupied_units + size <= magazyn->capacity_N) {
            magazyn->occupied_units += size;
            magazyn->count[component_type]++;
            magazyn->supplier_stats[component_type]++; 
            magazyn->supplier_status[component_type] = 1; 
            char buf[100];
            sprintf(buf, "Dostarczono skladnik %c. Stan: %d/%d", 'A' + component_type, magazyn->occupied_units, magazyn->capacity_N);
            log_event(name, buf);
        } else {
            printf("[%s] Magazyn pełny! Oczekiwanie...\n", name);
        }

        // --- WYJŚCIE Z SEKCJI KRYTYCZNEJ ---
        semop(semid, &unlock_storage, 1);
        if (semop(semid, &unlock_storage, 1) == -1) {
            perror("[Dostawca] semop unlock error");
        break;
    }
    }

    // Odłączenie od pamięci dzielonej przed końcem
    cleanup_before_exit();

    return 0;
}

void signal_handler(int sig) {
    keep_running = 0;
    sleep(1);
    for(int i=0; i<4; i++) {
        magazyn->supplier_status[i] = 3;
    }
}

void cleanup_before_exit() {
    if (magazyn != NULL && magazyn != (void*)-1) {
        check_error(shmdt(magazyn), "[Dostawca] Błąd shmdt (odłączenie pamięci)");
    }
}