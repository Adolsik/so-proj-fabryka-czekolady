#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include "shared.h"

WarehouseState *magazyn = NULL;
/**
 * @brief Flaga sterująca pętlą główną procesu, odporna na optymalizacje kompilatora.
 * * Wykorzystanie 'volatile' wymusza na procesorze każdorazowy odczyt wartości z pamięci RAM, 
 * co jest kluczowe, gdy flaga jest zmieniana asynchronicznie przez handler sygnału.
 * Typ 'sig_atomic_t' zapewnia, że operacje na tej zmiennej są niepodzielne (atomowe), 
 * co eliminuje ryzyko odczytu stanu nieustalonego w trakcie obsługi sygnału.
 */
volatile sig_atomic_t keep_working = 1;

void cleanup_before_exit();

void signal_handler(int sig);

/**
 * @brief Definicje operacji na semaforze (P i V).
 * * lock_storage: Operacja 'P'. Zmniejsza wartość semafora o 1. 
 * Jeśli semafor wynosi 0, proces zostaje wstrzymany (blokada sekcji krytycznej).
 * unlock_storage: Operacja 'V'. Zwiększa wartość semafora o 1. 
 * Budzi procesy oczekujące na dostęp do zasobu.
 */
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
    keep_working = 0;
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
        check_error(shmdt(magazyn), "[Pracownik] Błąd shmdt (odłączenie pamięci)");
    }
}