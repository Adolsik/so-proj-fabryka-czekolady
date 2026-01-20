#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include "shared.h"

WarehouseState *magazyn = NULL;
volatile sig_atomic_t keep_running = 1;

void cleanup_before_exit();

void signal_handler(int sig);

/**
 * @brief Definicje operacji na semaforze (P i V).
 * * lock_storage: Operacja 'P' (proszę). Zmniejsza wartość semafora o 1. 
 * Jeśli semafor wynosi 0, proces zostaje wstrzymany (blokada sekcji krytycznej).
 * unlock_storage: Operacja 'V' (wolne). Zwiększa wartość semafora o 1. 
 * Budzi procesy oczekujące na dostęp do zasobu.
 */
struct sembuf lock_storage = {0, -1, 0}; // sem_num,sem_op,sem_flg
struct sembuf unlock_storage = {0, 1, 0}; 


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
            usleep(750000 + (rand() % 750000)); // Dostawy A, B co 0.75-1.5s
        } else {
            usleep(1500000 + (rand() % 750000)); // Dostawy C, D co 1.5-2.25 sekundy
        }

        // --- WEJŚCIE DO SEKCJI KRYTYCZNEJ ---
        if (semop(semid, &lock_storage, 1) == -1) {
            if (errno == EINTR) continue; // Przerwano sygnałem, sprawdź warunek pętli
            perror("[Dostawca] semop lock error"); break;
        }

        //  Blokada konfliktu C+D dla N < 7
        int conflict = 0;
        if (magazyn->capacity_N < 7) {
            // Jeśli jestem C (typ 2), nie wchodzę gdy jest już D (typ 3)
            if (component_type == 2 && magazyn->count[3] > 0) conflict = 1;
            // Jeśli jestem D (typ 3), nie wchodzę gdy jest już C (typ 2)
            if (component_type == 3 && magazyn->count[2] > 0) conflict = 1;
        }

        // Sprawdzenie miejsca w magazynie
        if (magazyn->count[component_type] < magazyn->max_per_type[component_type] && !conflict) {
            // Dostarczamy składnik
            magazyn->count[component_type]++;
            magazyn->occupied_units += size; 
            magazyn->supplier_stats[component_type]++;
            magazyn->supplier_status[component_type] = 1; 
            char buf[100];
            sprintf(buf, "Dostarczono skladnik %c. Stan: %d/%d", 'A' + component_type, magazyn->occupied_units, magazyn->capacity_N);
            log_event(name, buf);
        } else {
            magazyn->supplier_status[component_type] = 2;
            char buf[100];
            sprintf(buf, "[%s] Magazyn pełny! Oczekiwanie...\n", name);
            log_event(name, buf);
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

/**
 * @brief Obsługuje sygnały systemowe w celu kontrolowanego zatrzymania pętli procesu.
 * * Funkcja zmienia wartość flagi sterującej 'keep_running' na 0 po odebraniu sygnału 
 * (np. SIGUSR1 lub SIGUSR2). Pozwala to procesowi na bezpieczne dokończenie 
 * aktualnej iteracji, zwolnienie semaforów i poprawne zamknięcie zasobów 
 * zamiast nagłego przerwania działania przez system.
 * * @param sig Numer odebranego sygnału (przekazywany automatycznie przez jądro systemu).
 * @return void
 */
void signal_handler(int sig) {
    keep_running = 0;
}

/**
 * @brief Odłącza segment pamięci współdzielonej od przestrzeni adresowej procesu.
 * * Funkcja jest wywoływana przed zakończeniem działania procesu potomnego. 
 * Sprawdza poprawność wskaźnika 'magazyn', a następnie używa shmdt(), aby 
 * poinformować jądro systemu, że proces nie będzie już korzystał z danego 
 * segmentu. Jest to kluczowy element zapobiegania wyciekom pamięci i błędnym 
 * odwołaniom przy zamykaniu fabryki.
 * * @return void
 */
void cleanup_before_exit() {
    if (magazyn != NULL && magazyn != (void*)-1) {
        check_error(shmdt(magazyn), "[Dostawca] Błąd shmdt (odłączenie pamięci)");
    }
}