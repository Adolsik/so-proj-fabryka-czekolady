
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include "shared.h"
#include <poll.h>
#include <string.h>


#define STATE_FILE "magazyn.dat"

// Struktura do przechowywania PIDów, by wiedzieć kogo zamykać
typedef struct {
    pid_t suppliers[4];
    pid_t workers[2];
} FactoryPIDs;

struct sembuf lock_storage = {0, -1, 0}; // sem_num,sem_op,sem_flg
struct sembuf unlock_storage = {0, 1, 0}; 

FactoryPIDs factory;

WarehouseState *magazyn;

int shmid, semid;

int sA = 0, sB = 0, sC = 0, sD = 0;
int w1 = 0, w2 = 0;
int testnum = 0;

void setup_limits(WarehouseState *mag);

void cleanup(int shmid, int semid);

void handle_sigint(int sig);

void save_state(WarehouseState *state);

void load_state(WarehouseState *state);

void print_dashboard(WarehouseState *mag);
 
int main(int argc, char *argv[]) {

    if (signal(SIGINT, handle_sigint) == SIG_ERR) {
        perror("Nie udało się zarejestrować sygnału SIGINT");
        exit(EXIT_FAILURE);
    }

    // Pojemność magazynu podana przez użytkownika lub z pliku
    int N;

    // Prosty parser (uproszczony dla czytelności)
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--n") == 0) N = atoi(argv[++i]);
        if (strcmp(argv[i], "--suppliers") == 0) {
            sA = atoi(argv[++i]); sB = atoi(argv[++i]);
            sC = atoi(argv[++i]); sD = atoi(argv[++i]);
        }
        if (strcmp(argv[i], "--workers") == 0) {
            w1 = atoi(argv[++i]); w2 = atoi(argv[++i]);
        }
        if(strcmp(argv[i], "--testnum") == 0) {
            testnum = atoi(argv[++i]);
        }
    }


    // Inicjalizacja IPC (Shared Memory, Semaphores)
    key_t key_shm = get_shm_key(FTOK_PATH, SHM_ID);
    check_error(key_shm, "[Dyrektor] Błąd ftok (tworzenie klucza pamieci wspoldzielonej)");

    shmid = shmget(key_shm, sizeof(WarehouseState), IPC_CREAT | 0600);
    check_error(shmid, "[Dyrektor] Błąd shmget (tworzenie pamieci)");

    magazyn = (WarehouseState *)shmat(shmid, NULL, 0);
    check_error((int)(intptr_t)magazyn, "[Dyrektor] Błąd shmat (dolaczenie pamieci)");
    
      if(testnum == 4) {
        if (access(STATE_FILE, F_OK) == 0) {
            printf("[Dyrektor] Znaleziono zapisany stan magazynu. Wczytuję dane...\n");
            load_state(magazyn);
            N = magazyn->capacity_N; 
            printf("[Dyrektor] Pojemność odtworzona z pliku: %d\n", N);
        } else {
        magazyn->capacity_N = N;
        magazyn->occupied_units = 0;
        for (int i = 0; i < MAX_COMPONENTS; i++) {
            magazyn->count[i] = 0;
        }
        }
    } else {
    // Inicjalizacja nowej struktury
    magazyn->capacity_N = N;
    magazyn->occupied_units = 0;
        for (int i = 0; i < MAX_COMPONENTS; i++) {
            magazyn->count[i] = 0;
        }
    }

    setup_limits(magazyn);
    magazyn->is_open = 1; // Magazyn jest otwarty na start

    key_t key_sem = get_sem_key(FTOK_PATH, SEM_ID);
    check_error(key_sem, "[Dyrektor] Błąd ftok (tworzenie klucza semafora)");
    semid = semget(key_sem, 1, IPC_CREAT | 0600);
    check_error(semid, "[Dyrektor] Błąd semget (tworzenie semafora)");

    semctl(semid, 0, SETVAL, 1);
    check_error(semctl(semid, 0, SETVAL, 1), "[Dyrektor] Błąd semctl (ustawianie wartosci semafora)");

    // Uruchamianie dostawców
    int supplier_counts[4] = {sA, sB, sC, sD};

    for (int type = 0; type < 4; type++) {
        for (int i = 0; i < supplier_counts[type]; i++) {
            if(supplier_counts[type] == 0) continue;
            pid_t pid = fork();
            if (pid == 0) {
                char type_str[2], name[20];
                sprintf(type_str, "%d", type);
                sprintf(name, "Dostawca_%c", 'A' + i);
                execl("./supplier", "supplier", type_str, name, NULL);
                exit(0);
            } else {
                factory.suppliers[type] = pid;
            }
        }
    }

    // Uruchamianie pracownikow
    int employee_counts[2] = {w1, w2};

    for (int type = 0; type < 2; type++) {
        for (int i = 0; i < employee_counts[type]; i++) {
            pid_t pid = fork();
            if (pid == 0) {
                char type_str[2];
                sprintf(type_str, "%d", type + 1); 
                execl("./worker", "worker", type_str, NULL);
                exit(0);
            } else {
                factory.workers[type] = pid;
            }
        }
    }

    // Menu Dyrektora 
    struct pollfd fds = { .fd = STDIN_FILENO, .events = POLLIN };

    int cmd = 0;
    while (cmd != 5) {
        print_dashboard(magazyn);
        if (poll(&fds, 1, 500) > 0) {
            scanf("%d", &cmd);
            switch(cmd) {
                case 1: // Stop Fabryka (Pracownicy)
                    for(int i=0; i<2; i++) kill(factory.workers[i], SIGUSR1);
                    sleep(1); 
                    for (int i = 0; i < 2; i++) magazyn->worker_status[i] = 3;
                    printf("[Dyrektor] Fabryka przestaje pracować.\n");
                    log_event("DYREKTOR", "Zatrzymano pracę fabryki (Polecenie 1)");
                    break;
                case 2: // Stop Magazyn
                    magazyn->is_open = 0;
                    sleep(1); 
                    for (int i = 0; i < 2; i++) magazyn->worker_status[i] = 3;
                    for (int i = 0; i < 4; i++) magazyn->supplier_status[i] = 3;
                    printf("[Dyrektor] Magazyn został zablokowany. Procesy czekają...\n");
                    log_event("DYREKTOR", "Zablokowano dostep do magazynu (Polecenie 2)");
                    break;
                case 3: // Stop Dostawcy
                    for(int i=0; i<4; i++) kill(factory.suppliers[i], SIGUSR2);
                    sleep(1); 
                    for (int i = 0; i < 4; i++) magazyn->supplier_status[i] = 3;
                    printf("[Dyrektor] Dostawcy przestają dostarczać składniki.\n");
                    log_event("DYREKTOR", "Zatrzymano dostawców (Polecenie 3)");
                    break;
                case 4: // Stop Fabryka, Magazyn
                    for(int i=0; i<2; i++){
                        kill(factory.workers[i], SIGUSR1);
                        magazyn->worker_status[i] = 3;
                    } 
                    magazyn->is_open = 0;
                    semctl(semid, 0, SETVAL, 0);
                    check_error(semctl(semid, 0, SETVAL, 0), "Błąd semctl (ustawianie wartosci semafora na 0)");
                    sleep(1); 
                    printf("[Dyrektor] Fabryka i Magazyn przestaje pracować.\n");
                    log_event("DYREKTOR", "Zablokowano pracę fabryki i dostęp do magazynu (Polecenie 4)");
                    break;
                case 5: // Wyjście
                    for(int i=0; i<2; i++) kill(factory.workers[i], SIGUSR1);
                    sleep(1);
                    for (int i=0; i<2; i++) magazyn->worker_status[i] = 0;
                    for(int i=0; i<4; i++) kill(factory.suppliers[i], SIGUSR2);
                    sleep(1);
                    for (int i=0; i<4; i++) magazyn->supplier_status[i] = 0;
                    save_state(magazyn);
                    log_event("DYREKTOR", "Zapisano stan magazynu i zakończono działanie programu");
                    break;
        }
    }
}

    // Czekanie na zakończenie wszystkich dzieci i sprzątanie (zombie itp)
    while(wait(NULL) > 0);

    cleanup(shmid, semid);
    return 0;
}

/**
 * @brief Inicjalizuje limity ilościowe magazynu na podstawie proporcji wagowych.
 * * Funkcja przydziela miejsce w magazynie według wag: A(1), B(1), C(2), D(3).
 * Proces przebiega w trzech etapach:
 * 1. Rezerwacja po 1 sztuce każdego surowca (gwarancja możliwości produkcji).
 * 2. Obliczenie pozostałej wolnej przestrzeni (N - 7).
 * 3. Dystrybucja reszty miejsca do składników A i B (najwyższe zużycie).
 * * @param mag Wskaźnik do struktury pamięci współdzielonej.
 */
void setup_limits(WarehouseState *mag) {
    int n = mag->capacity_N;

    // Bazowy limit jednostek na składnik
    int unit = n / 7;

    // Przypisanie podstawowe wg wag (1, 1, 2, 3)
    if(n>=7) {
    mag->max_per_type[0] = unit * 1; // A
    mag->max_per_type[1] = unit * 1; // B
    mag->max_per_type[2] = unit * 2 / 2; // C 
    mag->max_per_type[3] = unit * 3 / 3; // D 
    } else {
        // Dla bardzo małych N przydzielamy po 1 sztuce każdego składnika
        for (int i = 0; i < MAX_COMPONENTS; i++) {
            mag->max_per_type[i] = 1;
        }
        return;
    }
    int remainder = n % 7;

    // Rozdzielamy resztę między A i B
    // Jeśli reszta to np. 3, dajemy 2 dla A i 1 dla B (lub odwrotnie)
    while (remainder > 0) {
        if (remainder > 0) {
            mag->max_per_type[0]++;
            remainder--;
        }
        if (remainder > 0) {
            mag->max_per_type[1]++; 
            remainder--;
        }
    }
}

/**
 * @brief Usuwa zasoby komunikacji międzyprocesowej (IPC) z systemu.
 * * Funkcja odpowiada za zwolnienie segmentów pamięci współdzielonej, zestawów 
 * semaforów. Wykorzystuje polecenia IPC_RMID, aby 
 * zasoby nie pozostawały w systemie po zakończeniu pracy programu. Zawiera 
 * mechanizm ignorowania błędu EINVAL na wypadek, gdyby zasób został już 
 * usunięty przez inny proces.
 * * @param shmid Identyfikator pamięci współdzielonej.
 * @param semid Identyfikator zestawu semaforów.
 * @return void
 */
void cleanup(int shmid, int semid) {
    if (shmctl(shmid, IPC_RMID, NULL) == -1 && errno != EINVAL) perror("Błąd cleanup(): shmctl");
    if (semctl(semid, 0, IPC_RMID) == -1 && errno != EINVAL) perror("Błąd cleanup(): semctl");
    printf("\n[Dyrektor] Zasoby IPC usunięte z systemu.\n");
}
#include <signal.h> // Musisz dodać ten nagłówek

/**
 * @brief Obsługuje sygnał SIGINT (Ctrl+C) w celu bezpiecznego zamknięcia programu.
 * * Przechwytuje sygnał przerwania, wymusza wywołanie funkcji czyszczącej
 * zasoby IPC i bezpiecznie zamyka program. Gwarantuje to, że po Ctrl+C
 * nie pozostaną w systemie aktywne semafory ani segmenty pamięci.
 * * @param sig Numer otrzymanego sygnału.
 */
void handle_sigint(int sig) {
    int count_suppliers = sA + sB + sC + sD;
    int count_workers = w1 + w2;
    for(int i=0; i<count_suppliers; i++) kill(factory.suppliers[i], SIGUSR2);
    for(int i=0; i<count_workers; i++) kill(factory.workers[i], SIGUSR1);
    save_state(magazyn);
    sleep(1);

    cleanup(shmid, semid);
    exit(0); 
}

/**
 * @brief Zapisuje aktualny stan struktur magazynowych do pliku binarnego.
 * * Funkcja wykonuje zrzut całej struktury WarehouseState do pliku o nazwie 
 * zdefiniowanej w STATE_FILE. Wykorzystuje tryb zapisu binarnego ("wb"), co 
 * pozwala na zachowanie spójności danych przy ponownym uruchomieniu fabryki.
 * * @param state Wskaźnik do struktury pamięci współdzielonej, która ma zostać zapisana.
 * @return void
 */
void save_state(WarehouseState *state) {
    FILE *f = fopen(STATE_FILE, "wb");
    if (f == NULL) {
        perror("Błąd save_state(): fopen ");
    }
    if (f) {
        fwrite(state, sizeof(WarehouseState), 1, f);
        fclose(f);
        printf("[Dyrektor] Stan magazynu zapisany do pliku.\n");
    }
}

/**
 * @brief Odczytuje stan magazynu z pliku binarnego przy starcie programu.
 * * Funkcja próbuje otworzyć plik stanu i wczytać zapisaną wcześniej strukturę 
 * WarehouseState bezpośrednio do pamięci współdzielonej. Umożliwia to 
 * kontynuację pracy fabryki z zachowaniem poprzednich statystyk i zapasów.
 * * @param state Wskaźnik do struktury pamięci współdzielonej, do której trafią dane.
 * @return void
 */
void load_state(WarehouseState *state) {
    FILE *f = fopen(STATE_FILE, "rb");
    if (f == NULL) {
        perror("Błąd load_state(): fopen ");
    }
    if (f) {
        fread(state, sizeof(WarehouseState), 1, f);
        fclose(f);
        printf("[Dyrektor] Stan magazynu odtworzony z pliku.\n");
    }
}

/**
 * @brief Wyświetla interaktywny panel monitorowania fabryki w terminalu.
 * * Funkcja generuje czytelny dashboard TUI, który w czasie rzeczywistym 
 * prezentuje stopień zapełnienia magazynu, statusy poszczególnych dostawców 
 * i pracowników oraz statystyki produkcji. Wykorzystuje sekwencje sterujące 
 * ANSI do odświeżania widoku bez przewijania ekranu (efekt dashboardu).
 * * @param mag Wskaźnik do pamięci współdzielonej zawierającej aktualne dane fabryki.
 * @return void
 */
void print_dashboard(WarehouseState *mag) {
    if (semop(semid, &lock_storage, 1) == -1) {
        perror("[Dyrektor] semop lock error"); 
    }
    printf("\033[H\033[J"); // Czyszczenie ekranu terminala
    printf("=====================================================================\n");
    printf(CLR_BOLD CLR_MAGENTA " Test nr. %d\n" CLR_RESET CLR_RESET, testnum);
    printf("=====================================================================\n");
    printf(" MAGAZYN: %d/%d [%s]\n", mag->occupied_units, mag->capacity_N, 
           mag->is_open ? CLR_GREEN "OTWARTY" CLR_RESET : CLR_RED "ZABLOKOWANY" CLR_RESET);
    
    printf("---------------------------------------------------------------------\n");
    printf(" DOSTAWCY (Składniki):\n");
    int count_suppliers = sA + sB + sC + sD;
    for(int i=0; i<count_suppliers; i++) {
        char *st = (mag->supplier_status[i] == 1) ? CLR_GREEN "Dostarcza" CLR_RESET : 
                   (mag->supplier_status[i] == 2) ? CLR_YELLOW "OCZEKUJE " CLR_RESET : CLR_RED "STOP" CLR_RESET;
        printf(" [%d] [%c] Dostarczono: %3d szt. | Status: %s |", factory.suppliers[i], 'A'+i, mag->supplier_stats[i], st);
        printf(" W magazynie: %d/%d szt.", mag->count[i], mag->max_per_type[i]);
        printf("\n");
            }

    printf("---------------------------------------------------------------------\n");
    printf(" PRACOWNICY (Produkcja):\n");
    int count_workers = w1 + w2;
    for(int i=0; i<count_workers; i++) {
        char *st = (mag->worker_status[i] == 1) ? CLR_GREEN "Produkuje" CLR_RESET : 
                   (mag->worker_status[i] == 2) ? CLR_YELLOW "BRAK SKŁ." CLR_RESET : CLR_RED "STOP" CLR_RESET;
        printf(" [%d] [%d] Wyprodukowano: %3d czek. | Status: %s\n", factory.workers[i], i+1, mag->worker_stats[i], st);
    }
    printf("=====================================================================\n");
    printf(" 1: Stop Fabryka | 2:  Stop Magazyn | 3: Stop Dostawcy\n");
    printf(" 4: Stop Fabryka i Magazyn | 5: Zakończ i zapisz stan\n");
    printf(" Wybór: ");
    fflush(stdout);

    if (semop(semid, &unlock_storage, 1) == -1) {
        perror("[Dyrektor] semop unlock error");
    }  
}