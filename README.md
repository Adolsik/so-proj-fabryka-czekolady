# 🍫 Temat 3 – Fabryka Czekolady 

## 🎯 Cel Projektu

Implementacja symulacji procesów w fabryce czekolady, koncentrująca się na zarządzaniu współbieżnym dostępem do **magazynu surowców** i koordynacją pracy **dostawców** oraz **stanowisk produkcyjnych**.

## ⚙️ Opis Procesu

Fabryka produkuje **dwa rodzaje czekolady**:

1.  **Czekolada Typu 1 (Stanowisko 1):** Wymaga składników **A, B i C**.
2.  **Czekolada Typu 2 (Stanowisko 2):** Wymaga składników **A, B i D**.

---

### 📦 Magazyn Surowców

* **Pojemność:** Magazyn ma ograniczoną pojemność **N jednostek**.
* **Wymagania Składników (Jednostki Magazynowe):**
    * Składnik **A:** 1 jednostka.
    * Składnik **B:** 1 jednostka.
    * Składnik **C:** 2 jednostki.
    * Składnik **D:** 3 jednostki.
* **Polityka Zarządzania:** Fabryka dąży do przyjęcia **maksymalnie dużo** podzespołów, aby zachować **płynność produkcji**.

### 🚚 Dostawy

* **Źródła:** Składniki **A, B, C i D** pochodzą z **4 niezależnych źródeł**.
* **Częstotliwość:** Dostawy odbywają się w **nieokreślonych momentach czasowych** (asynchronicznie).

### 🏭 Produkcja

* **Pobieranie:** Pracownicy pobierają składniki z magazynu i przenoszą je na swoje stanowisko (1 lub 2).
* **Współbieżność:** Procesy produkcji (pobieranie/użycie) i dostaw trwają **jednocześnie**.

---

## 🛑 Warunki Końcowe i Sterowanie

Procesy są sterowane przez **Dyrektora** (proces `dyrektor`), który wydaje specjalne polecenia:

| Polecenie | Odbiorca | Efekt |
| :---: | :--- | :--- |
| `polecenie_1` | **Fabryka** | Kończy pracę (produkcja ustaje). |
| `polecenie_2` | **Magazyn** | Kończy pracę (nie przyjmuje i nie wydaje składników). |
| `polecenie_3` | **Dostawcy** | Przerywają dostawy. |
| `polecenie_4` | **Fabryka i Magazyn** | Kończą pracę jednocześnie. Aktualny stan magazynu **zostaje zapisany do pliku**. Po ponownym uruchomieniu stan magazynu **jest odtwarzany z pliku**. |

---

## 💻 Wymagane Procesy i Implementacja

Należy zaimplementować następujące procesy (w architekturze wieloprocesowej lub wielowątkowej):

1.  **`dyrektor`:** Odpowiedzialny za wysyłanie poleceń sterujących (`polecenie_1` do `polecenie_4`).
2.  **`dostawca`:** **Cztery** niezależne procesy (po jednym dla A, B, C, D) symulujące dostawę składników do magazynu.
3.  **`pracownik`:** **Dwa** niezależne procesy (Stanowisko 1 i Stanowisko 2) symulujące pobieranie surowców i produkcję czekolady.

### 📝 Raportowanie

* Raport z przebiegu całej symulacji (dostawy, pobrania, zmiany stanu magazynu, zdarzenia sterujące) należy **zapisać w pliku (lub plikach) tekstowym**.
