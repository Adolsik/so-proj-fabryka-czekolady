#ifndef SHARED_H
#define SHARED_H

#define KEY_SHM 1234
#define KEY_SEM 5678
#define KEY_MSG 9012
#define MAX_COMPONENTS 4 // A, B, C, D

// Indeksy składników
enum Component { A = 0, B = 1, C = 2, D = 3 };

// Struktura Magazynu w Pamięci Dzielonej
typedef struct {
    int capacity_N; // Całkowita pojemność N
    int occupied_units; // Aktualnie zajęte jednostki
    int count[MAX_COMPONENTS]; // Liczba sztuk składników A, B, C, D
} WarehouseState;

// Struktura Komunikatu (dla Dyrektora)
// Typ komunikatu 1: Polecenia sterujące
typedef struct {
    long mtype; // 1
    int command; // 1, 2, 3, lub 4 (Polecenia Dyrektora)
} DirectorCommand;

// Jednostki zajmowane przez składniki
const int component_size[] = {1, 1, 2, 3}; // A=1, B=1, C=2, D=3

#endif 