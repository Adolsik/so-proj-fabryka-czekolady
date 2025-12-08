#ifndef COMMON_H
#define COMMON_H



// --- UNION dla semctl (Niezbędne dla używania SETALL/SETVAL) ---
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
    struct seminfo *__buf;
};

// --- Stałe systemu ---
#define MAX_CAPACITY_N 20 // Maksymalna pojemność magazynu

// --- Indeksy składników w tablicach (ułatwia kod) ---
#define A_INDEX 0
#define B_INDEX 1
#define C_INDEX 2
#define D_INDEX 3
#define NUM_INGREDIENTS 4

const int INGREDIENT_WEIGHTS[NUM_INGREDIENTS] = {1, 1, 2, 3};

// --- Indeksy Semaforów (w zestawie) ---
#define MUTEX_SEM 0      // Blokuje dostęp do Pamięci Dzielonej (dla atomowości)
#define EMPTY_SEM 1      // Liczy wolne miejsce w magazynie (dla Dostawców)
#define FULL_A_SEM 2     // Dostępność A (
#define FULL_B_SEM 3     // Dostępność B
#define FULL_C_SEM 4     // Dostępność C
#define FULL_D_SEM 5     // Dostępność D
#define NUM_SEMS 6       // Łączna liczba semaforów

// --- Struktura Pamięci Dzielonej (Magazyn) ---
typedef struct {
    int inventory[NUM_INGREDIENTS]; 
    int current_load;               
} WarehouseState;




#endif 