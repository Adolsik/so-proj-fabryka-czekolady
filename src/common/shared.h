#ifndef SHARED_H
#define SHARED_H

#define KEY_SHM 1234
#define KEY_SEM 5678
#define KEY_MSG 9012
#define MAX_COMPONENTS 4 // A, B, C, D

#include <time.h>

// Indeksy składników
enum Component { A = 0, B = 1, C = 2, D = 3 };

// Struktura Magazynu w Pamięci Dzielonej
typedef struct {
    int capacity_N; // Całkowita pojemność N
    int occupied_units; // Aktualnie zajęte jednostki
    int count[MAX_COMPONENTS]; // Liczba sztuk składników A, B, C, D
    int is_open; // Flaga czy magazyn jest otwarty (Polecenie zamknięcia magazynu)
} WarehouseState;

// Jednostki zajmowane przez składniki
static const int component_size[] = {1, 1, 2, 3}; // A=1, B=1, C=2, D=3

static inline void log_event(const char *process_name, const char *message) {
    FILE *f = fopen("fabryka_raport.txt", "a");
    if (f == NULL) {
        perror("Błąd otwarcia pliku raportu");
        return;
    }

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    
    // Format: [HH:MM:SS] [NazwaProcesu] Wiadomość
    fprintf(f, "[%02d:%02d:%02d] [%s] %s\n", 
            t->tm_hour, t->tm_min, t->tm_sec, process_name, message);
    
    fclose(f);
}


#endif 