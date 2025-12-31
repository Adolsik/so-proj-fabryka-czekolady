#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include "shared.h"

WarehouseState *magazyn = NULL;

void cleanup_before_exit();

// Zmienna sterująca pętlą (obsługa sygnałów)
volatile sig_atomic_t keep_working = 1;

void signal_handler(int sig);

// Operacje na semaforze (Muteks)
struct sembuf lock_storage = {0, -1, 0};
struct sembuf unlock_storage = {0, 1, 0};

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Błąd: Brak typu pracownika (1 lub 2).\n");
        exit(EXIT_FAILURE);
    }

    int worker_type = atoi(argv[1]); // 1 lub 2
    
    // Rejestracja sygnałów Polecenie stop Pracownik
    signal(SIGUSR1, signal_handler);

    // Podłączenie do IPC
    int shmid = shmget(KEY_SHM, sizeof(WarehouseState), 0600);
    check_error(shmid, "[Pracownik] Błąd shmget (dostęp do pamięci)");
    magazyn = (WarehouseState *)shmat(shmid, NULL, 0);
    check_error((int)(intptr_t)magazyn, "[Pracownik] Błąd shmat (dołączenie pamięci)");

    int semid = semget(KEY_SEM, 1, 0600);
    check_error(semid, "[Pracownik] Błąd semget (dostęp do semafora)");

    printf("[Pracownik %d] Rozpoczynam linię produkcyjną typu %d.\n", worker_type, worker_type);

    while (keep_working) {
         if (!magazyn->is_open) {
            sleep(1); continue; // Magazyn zamknięty proces czeka
        }

        // Czekaj od 1 do 3 sekund losowo przed każdą próbą (process starvation)
        sleep(1);
        usleep((rand() % 2000) * 1000);

        // --- WEJŚCIE DO SEKCJI KRYTYCZNEJ (dostęp atomowy) ---
        if (semop(semid, &lock_storage, 1) == -1) {
            if (errno == EINTR) continue;
            perror("[Pracownik] semop lock error"); break;
        }

        int can_produce = 0;
        int total_size_to_free = 0;

        // Logika sprawdzania składników
        if (worker_type == 1) {
            // Potrzebuje A (idx 0), B (idx 1), C (idx 2)
            if (magazyn->count[0] > 0 && magazyn->count[1] > 0 && magazyn->count[2] > 0) {
                magazyn->count[0]--;
                magazyn->count[1]--;
                magazyn->count[2]--;
                total_size_to_free = component_size[0] + component_size[1] + component_size[2];
                can_produce = 1;
            }
        } else if (worker_type == 2) {
            // Potrzebuje A (idx 0), B (idx 1), D (idx 3)
            if (magazyn->count[0] > 0 && magazyn->count[1] > 0 && magazyn->count[3] > 0) {
                magazyn->count[0]--;
                magazyn->count[1]--;
                magazyn->count[3]--;
                total_size_to_free = component_size[0] + component_size[1] + component_size[3];
                can_produce = 1;
            }
        }

        if (can_produce) {
            magazyn->occupied_units -= total_size_to_free;
            char buf[100];
            magazyn->worker_stats[worker_type-1]++; // Zwiększamy licznik wyprodukowanych czekolad
            magazyn->worker_status[worker_type-1] = 1; // "Produkcja zakończona"
            sprintf(buf, "Wyprodukowano czekolade typu %d. Wolne miejsce: %d", worker_type, magazyn->capacity_N - magazyn->occupied_units);
            log_event((worker_type == 1 ? "Pracownik_1" : "Pracownik_2"), buf);
        } else {
            magazyn->worker_status[worker_type-1] = 2;
        }
             
        // --- WYJŚCIE Z SEKCJI KRYTYCZNEJ ---
        semop(semid, &unlock_storage, 1);
        if (semop(semid, &unlock_storage, 1) == -1) {
            perror("[Pracownik] semop unlock");
            break;
        }
    }

    cleanup_before_exit();
    return 0;
}

void signal_handler(int sig) {
    keep_working = 0;
    sleep(1);
    for(int i=0; i<2; i++) {
        magazyn->worker_status[i] = 3;
    }
}

void cleanup_before_exit() {
    if (magazyn != NULL && magazyn != (void*)-1) {
        check_error(shmdt(magazyn), "[Pracownik] Błąd shmdt (odłączenie pamięci)");
    }
}