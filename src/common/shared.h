#ifndef SHARED_H
#define SHARED_H

#define CLR_RESET  "\x1b[0m"
#define CLR_RED    "\x1b[31m"
#define CLR_GREEN  "\x1b[32m"
#define CLR_YELLOW "\x1b[33m"
#define CLR_MAGENTA "\x1b[35m"
#define CLR_BOLD   "\x1b[1m"

#define KEY_SHM 1234
#define KEY_SEM 5678
#define MAX_COMPONENTS 4 // A, B, C, D

#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

// Indeksy składników
enum Component { A = 0, B = 1, C = 2, D = 3 };

// Struktura Magazynu w Pamięci Dzielonej
typedef struct {
    int capacity_N; // Całkowita pojemność N
    int occupied_units; // Aktualnie zajęte jednostki
    int max_per_type[MAX_COMPONENTS]; // Maksymalna liczba sztuk każdego składnika
    int count[MAX_COMPONENTS]; // Liczba sztuk składników A, B, C, D
    int is_open; // Flaga czy magazyn jest otwarty (Polecenie zamknięcia magazynu)

    // Nowe statystyki
    int supplier_stats[4];  // Ile sztuk dostarczył Dostawca A, B, C, D
    int worker_stats[2];    // Ile czekolad zrobił Pracownik 1, Pracownik 2
    
    // Statusy (0 - Nieaktywny, 1 - Pracuje, 2 - Czeka na miejsce/składniki, 3 - Zatrzymany sygnałem)
    int supplier_status[4];
    int worker_status[2];
} WarehouseState;

// Jednostki zajmowane przez składniki
static const int component_size[] = {1, 1, 2, 3}; // A=1, B=1, C=2, D=3


/**
 * @brief Zapisuje zdarzenie systemowe do pliku tekstowego z oznaczeniem czasu.
 * * Funkcja otwiera plik raportu w trybie dopisywania ("a"), pobiera aktualny czas
 * systemowy i formatuje wpis tak, aby zawierał dokładną godzinę oraz nazwę procesu,
 * który wywołał zdarzenie. Zapewnia to pełną historię operacji fabryki w celach diagnostycznych.
 * * @param process_name Nazwa procesu generującego wpis (np. "Dyrektor", "Dostawca_A").
 * @param message Treść komunikatu opisującego zdarzenie.
 * @return void
 */
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

/**
 * @brief Sprawdza wynik operacji systemowej i przerywa program w przypadku błędu.
 * * Funkcja służy do centralnej obsługi błędów funkcji systemowych (np. shmget, semop).
 * Jeśli parametr 'result' wynosi -1, funkcja wypisuje opis błędu przy użyciu perror()
 * i natychmiast kończy działanie programu z kodem EXIT_FAILURE, zapobiegając
 * niestabilnemu działaniu procesów potomnych.
 * * @param result Wynik sprawdzanej funkcji systemowej (zazwyczaj int lub pid_t).
 * @param msg Komunikat opisujący kontekst błędu, który zostanie wyświetlony w terminalu.
 * @return void
 */
static inline void check_error(int result, const char *msg) {
    if (result == -1) {
        perror(msg);
        exit(EXIT_FAILURE);
    }
}

#endif 