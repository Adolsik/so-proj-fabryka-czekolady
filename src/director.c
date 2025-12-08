#include "common/common.h" // Zawiera definicje stałych
#include <unistd.h>      // fork(), exec(), getpid()
#include <stdio.h>
#include <stdlib.h>
#include <string.h>   
#include <errno.h>

#include <sys/sem.h>
#include <sys/wait.h>   // waitpid()
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>

void cleanup_ipc(int shmid, int semid, int msgid) {
    printf("[Dyrektor %d] Czyszczenie struktur IPC...\n", getpid());
    
    ///TODO

    printf("[Dyrektor %d] Czyszczenie zakończone.\n", getpid());
}

int main(int argc, char *argv[]) {
    // walidacja argumentów TODO

    // 1. Pamięć Dzielona (Magazyn)
    key_t shm_key = ftok(".", 10);
    int shmid = shmget(shm_key, sizeof(WarehouseState), IPC_CREAT | 0600);
    //if (shmid == -1) handle_sys_error("shmget failed");

    // 2. Zestaw Semaforów (Synchronizacja)
    key_t sem_key = ftok(".", 11);
    int semid = semget(sem_key, NUM_SEMS, IPC_CREAT | 0600);
    //if (semid == -1) handle_sys_error("semget failed");

    // 3. Kolejka Komunikatów (Sterowanie)
    key_t msg_key = ftok(".", 12);
    int msgid = msgget(msg_key, IPC_CREAT | 0600);
    //if (msgid == -1) handle_sys_error("msgget failed");

    // 4. Dołączenie do SHM i inicjalizacja stanu (tylko Dyrektor)
    WarehouseState *wh = (WarehouseState *)shmat(shmid, NULL, 0);
    //if (wh == (void *)-1) handle_sys_error("shmat failed");
    
    // Wyzerowanie magazynu na start
    memset(wh->inventory, 0, sizeof(wh->inventory));
    wh->current_load = 0;
    
    //if (shmdt(wh) == -1) handle_sys_error("shmdt failed");

    unsigned short init_sem_values[NUM_SEMS] = {
        1,                    // MUTEX_SEM: Początkowo wolny (1)
        MAX_CAPACITY_N,       // EMPTY_SEM: Początkowo jest N wolnych jednostek
        0, 0, 0, 0            // FULL_A/B/C/D_SEM: Początkowo puste (0)
    };

    // Wymagana jest unia do inicjalizacji semaforów
    union semun su;
    su.array = init_sem_values;
    
    // if (semctl(semid, 0, SETALL, su) == -1) handle_sys_error("semctl SETALL failed");

    pid_t pid;
    
    // Uruchomienie 4 Dostawców (A, B, C, D)
    for (int i = 0; i < NUM_INGREDIENTS; i++) {
        if ((pid = fork()) == 0) {
            char ingredient_index_str[2];
            sprintf(ingredient_index_str, "%d", i);
            
            // execv wymaga tablicy argumentów
            char *args[] = {"./supplier", ingredient_index_str, NULL};
            execv(args[0], args);
            // Jeśli execv się nie powiedzie, zwróć błąd
            //handle_sys_error("execv supplier failed");
        }
    }

    // Uruchomienie 2 Pracowników (Stanowisko 1 i 2)
    for (int i = 1; i <= 2; i++) {
        if ((pid = fork()) == 0) {
            char station_id_str[2];
            sprintf(station_id_str, "%d", i);

            char *args[] = {"./worker", station_id_str, NULL};
            execv(args[0], args);
           // handle_sys_error("execv worker failed");
        }
    }

    // Oczekiwanie na zakończenie wszystkich procesów potomnych
    while (wait(NULL) > 0);

    // Czyszczenie zasobów IPC przed zakończeniem
    cleanup_ipc(shmid, semid, msgid);

    return 0;
}