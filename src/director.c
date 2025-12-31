
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include "shared.h"
#include <poll.h>


#define STATE_FILE "magazyn.dat"

// Struktura do przechowywania PIDów, by wiedzieć kogo zamykać
typedef struct {
    pid_t suppliers[4];
    pid_t workers[2];
} FactoryPIDs;

// Funkcja czyszcząca zasoby IPC
void cleanup(int shmid, int semid, int msqid);

// Zapisywanie stanu magazynu
void save_state(WarehouseState *state);

// Wczytywanie stanu magazynu
void load_state(WarehouseState *state);

// Interfejs
void print_dashboard(WarehouseState *mag);
 
int main() {
    int N;

    // Inicjalizacja IPC (Shared Memory, Semaphores)
    int shmid = shmget(KEY_SHM, sizeof(WarehouseState), IPC_CREAT | 0600);
    check_error(shmid, "[Dyrektor] Błąd shmget (tworzenie pamieci)");

    WarehouseState *magazyn = (WarehouseState *)shmat(shmid, NULL, 0);
    check_error((int)(intptr_t)magazyn, "[Dyrektor] Błąd shmat (dolaczenie pamieci)");
    
    if (access(STATE_FILE, F_OK) == 0) {
        printf("[Dyrektor] Znaleziono zapisany stan magazynu. Wczytuję dane...\n");
        load_state(magazyn);
        N = magazyn->capacity_N; 
        printf("[Dyrektor] Pojemność odtworzona z pliku: %d\n", N);
    } else {
        printf("[Dyrektor] Brak zapisanego stanu. Podaj pojemność magazynu N: ");
        if (scanf("%d", &N) != 1 || N < 20) {
            fprintf(stderr, "Błąd: Nie ma magazynow z taką małą pojemnością.\n");
            exit(EXIT_FAILURE);
        }
        
        // Inicjalizacja nowej struktury
        magazyn->capacity_N = N;
        magazyn->occupied_units = 0;
        for (int i = 0; i < MAX_COMPONENTS; i++) {
            magazyn->count[i] = 0;
        }
        printf("[Dyrektor] Zainicjalizowano nowy magazyn o pojemności: %d\n", N);
    }

    magazyn->is_open = 1; // Magazyn jest otwarty na start

    int semid = semget(KEY_SEM, 1, IPC_CREAT | 0600);
    check_error(semid, "[Dyrektor] Błąd semget (tworzenie semafora)");

    semctl(semid, 0, SETVAL, 1);
    check_error(semctl(semid, 0, SETVAL, 1), "[Dyrektor] Błąd semctl (ustawianie wartosci semafora)");

    int msqid = msgget(KEY_MSG, IPC_CREAT | 0600);
    check_error(msqid, "[Dyrektor] Błąd msgget (tworzenie kolejki komunikatów)");

    FactoryPIDs factory;

    // Uruchamianie 4 Dostawców
    for (int i = 0; i < 4; i++) {
        pid_t p = fork();
        check_error(p, "[Dyrektor] Błąd fork (tworzenie dostawcy)");
        if (p == 0) {
            char type[2], name[20];
            sprintf(type, "%d", i);
            sprintf(name, "Dostawca_%c", 'A' + i);
            execl("./supplier", "./supplier", type, name, NULL);
            check_error(execl("./supplier", "./supplier", type, name, NULL), "[Dyrektor] Błąd execl (uruchamianie dostawcy)");
            exit(1);
        } else {
            factory.suppliers[i] = p;
        } 
    }

    // Uruchamianie 2 Pracowników
    for (int i = 0; i < 2; i++) {
        pid_t p = fork();
        check_error(p, "[Dyrektor] Błąd fork (tworzenie pracownika)");
        if (p == 0) {
            char type[2];
            sprintf(type, "%d", i + 1);
            execl("./worker", "./worker", type, NULL);
            check_error(execl("./worker", "./worker", type, NULL), "[Dyrektor] Błąd execl (uruchamianie pracownika)");
            exit(1);
        } else {
            factory.workers[i] = p;
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
                    printf("[Dyrektor] Fabryka przestaje pracować.\n");
                    log_event("DYREKTOR", "Zatrzymano pracę fabryki (Polecenie 1)");
                    break;
                case 2: // Stop Magazyn
                    magazyn->is_open = 0;
                    semctl(semid, 0, SETVAL, 0);
                    check_error(semctl(semid, 0, SETVAL, 0), "Błąd semctl (ustawianie wartosci semafora na 0)");
                    sleep(1); 
                    printf("[Dyrektor] Magazyn został zablokowany. Procesy czekają...\n");
                    log_event("DYREKTOR", "Zablokowano dostep do magazynu (Polecenie 2)");
                    break;
                case 3: // Stop Dostawcy
                    for(int i=0; i<4; i++) kill(factory.suppliers[i], SIGUSR2);
                    sleep(1); 
                    printf("[Dyrektor] Dostawcy przestają dostarczać składniki.\n");
                    log_event("DYREKTOR", "Zatrzymano dostawców (Polecenie 3)");
                    break;
                case 4: // Stop Fabryka, Magazyn
                    for(int i=0; i<2; i++) kill(factory.workers[i], SIGUSR1);
                    magazyn->is_open = 0;
                    semctl(semid, 0, SETVAL, 0);
                    check_error(semctl(semid, 0, SETVAL, 0), "Błąd semctl (ustawianie wartosci semafora na 0)");
                    sleep(1); 
                    printf("[Dyrektor] Fabryka i Magazyn przestaje pracować.\n");
                    log_event("DYREKTOR", "Zablokowano pracę fabryki i dostęp do magazynu (Polecenie 4)");
                    break;
                case 5: // Wyjście
                    for(int i=0; i<2; i++) kill(factory.workers[i], SIGUSR1);
                    for(int i=0; i<4; i++) kill(factory.suppliers[i], SIGUSR2);
                    sleep(1); 
                    save_state(magazyn);
                    log_event("DYREKTOR", "Zapisano stan magazynu i zakończono działanie programu");
                    break;
        }
    }
}

    // Czekanie na zakończenie wszystkich dzieci i sprzątanie (zombie itp)
    while(wait(NULL) > 0);

    cleanup(shmid, semid, msqid);
    return 0;
}

void cleanup(int shmid, int semid, int msqid) {
    shmctl(shmid, IPC_RMID, NULL);
    if (shmctl(shmid, IPC_RMID, NULL) == -1 && errno != EINVAL) perror("Błąd cleanup(): shmctl");
    semctl(semid, 0, IPC_RMID);
    if (semctl(semid, 0, IPC_RMID) == -1 && errno != EINVAL) perror("Błąd cleanup(): semctl");
    msgctl(msqid, IPC_RMID, NULL);
    if (msgctl(msqid, IPC_RMID, NULL) == -1 && errno != EINVAL) perror("Błąd cleanup(): msgctl");
    printf("\n[Dyrektor] Zasoby IPC usunięte z systemu.\n");
}

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

void print_dashboard(WarehouseState *mag) {
    printf("\033[H\033[J"); 
    printf("====================================================\n");
    printf("        MONITOR FABRYKI CZEKOLADY      \n");
    printf("====================================================\n");
    printf(" MAGAZYN: %d/%d [%s]\n", mag->occupied_units, mag->capacity_N, 
           mag->is_open ? "OTWARTY" : "ZABLOKOWANY");
    
    printf("----------------------------------------------------\n");
    printf(" DOSTAWCY (Składniki):\n");
    for(int i=0; i<4; i++) {
        char *st = (mag->supplier_status[i] == 1) ? "Dostarcza" : 
                   (mag->supplier_status[i] == 2) ? "OCZEKUJE " : "STOP     ";
        printf(" [%c] Dostarczono: %3d szt. | Status: %s |", 'A'+i, mag->supplier_stats[i], st);
        // Mały pasek zapasów tego typu
        for(int j=0; j<mag->count[i]; j++) printf("#");
        printf("\n");
    }

    printf("----------------------------------------------------\n");
    printf(" PRACOWNICY (Produkcja):\n");
    for(int i=0; i<2; i++) {
        char *st = (mag->worker_status[i] == 1) ? "Produkuje" : 
                   (mag->worker_status[i] == 2) ? "BRAK SKŁ." : "STOP     ";
        printf(" [%d] Wyprodukowano: %3d czek. | Status: %s\n", i+1, mag->worker_stats[i], st);
    }
    printf("====================================================\n");
    printf(" 1: Stop Fabryka | 2:  Stop Magazyn | 3: Stop Dostawcy\n");
    printf(" 4: Stop Fabryka i Magazyn | 5: Zakończ i zapisz stan\n");
    printf(" Wybór: ");
    fflush(stdout);
}