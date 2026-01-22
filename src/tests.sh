make

echo "TESTY PROJEKTU"
echo "1. Test blokowania (N=10, Dostawca A,B,C,D)"
echo "2. Test płynności (C przed D)"
echo "3. Test braku składnika D"
echo "4. Test nagłego zamknięcia (SIGINT)"
read -p "Wybierz numer testu: " test_no

case $test_no in
  1)
     # Test 1: Czy pełny magazyn blokuje dostawców?
     # Ustawiamy mały magazyn (N=10) i dostawców i sprawdzamy czy któryś z nich przekroczy limit magazynu. 

     # Wyjaśnienie dlaczego ten test działa:
     # W kodzie dostawcy mamy warunek:
     # if (magazyn->count[component_type] < magazyn->max_per_type[component_type] && magazyn->occupied_units + size <= magazyn->capacity_N && !conflict)
     # Ten warunek sprawdza, czy dostawca może dostarczyć składnik. Jeśli dodany składnik miałby przekroczyć pojemność magazynu (occupied_units + size <= capacity_N),
     # to dostawca nie będzie mógł dostarczyć składnika
    ./directortest --n 10 --suppliers 1 1 1 1 --workers 0 0 --testnum 1
    ;;
  2)
    # Test 2:
    # Magazyn jest pełny. Zablokowani są dostawcy C i D. Pracownik 1 zużywa składniki. Dostawca z mniejszym zapotrzebowaniem na miejsce 
    # (np. C – 2 jedn.) powinien być obsłużony w pierwszej kolejności przed dostawcą D - 3 jedn. jeśli zwolni się tylko tyle miejsca.

    # Wyjaśnienie dlaczego ten test działa:
    # W kodzie mamy ponownie ifa (occupied_units + size <= capacity_N),
    # Jeśli magazyn jest zajęty 8/10 jednostek to gdy dostawca D(3 jednostki) będzie chciał dostarczyć składnik to warunek nie zostanie spełniony (8 + 3 <= 10).
    # wtedy nie będzie czekał na zwolnienie miejsca tylko zwolni semafor i pozowoli innemu dostawcy dostarczyć składnik. A jeśli C w tym momencie chciałby dostarczyć składnik
    # to warunek zostanie spełniony (8 + 2 <= 10) i dostawca C dostarczy składnik.
    ./directortest --n 10 --suppliers 1 1 1 1 --workers 1 1 --testnum 2
    ;;
  3)
    # Test 3: Brak dostawcy D pracuje tylko 1 pracownik a pracownik 2 nie może zabierać składników A,B jeśli nie ma składnika D

    # Wyjaśnienie dlaczego ten test działa: 
    # Pracownik wchodząc do sekcji krytycznej sprawdza czy są dostępne wszystkie składniki potrzebne do produkcji czekolady. Jeśli brakuje nawet jednego składnika
    # to pracownik nie zabiera żadnych składników i wychodzi z sekcji krytycznej. W takim razie nie ma możliwości aby pracownik 2 zabrał składniki A i B jeśli nie ma składnika D.
    ./directortest --n 20 --suppliers 1 1 1 0 --workers 1 1 --testnum 3
    ;;
  4)
    # Test 4: Nagłe zamknięcie (SIGINT)

    # Wyjaśnienie dlaczego ten test działa:
    # W kodzie dyrektora mamy obsługę sygnału SIGINT, która wysyła sygnały zakończenia do wszystkich procesów dostawców i pracowników oraz zapisuje aktualny stan magazynu do pliku.
    # Dzięki temu, gdy dyrektor zostanie nagle zamknięty, wszystkie procesy potomne zostaną poprawnie zakończone, a stan magazynu zostanie zachowany.
    ./directortest --n 20 --suppliers 1 1 1 1 --workers 1 1 --testnum 4
    ;;
esac