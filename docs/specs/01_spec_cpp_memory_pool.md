# Software Specification: High-Performance Memory Pool Manager (C/C++)

## 1. Obiettivo & Business Context
Molti sistemi ad alte prestazioni (es. motori grafici, server di trading finanziario, database) risentono della frammentazione della memoria e del sovraccarico generato dalle frequenti chiamate a `malloc`/`free` o `new`/`delete`. 
Questo componente ha lo scopo di fornire un **allocatore di memoria personalizzato** (Memory Pool) che pre-alloca un blocco continuo di memoria, gestendo l'allocazione e la deallocazione di blocchi di dimensioni fisse a tempo costante $O(1)$ con zero frammentazione esterna.

---

## 2. Requisiti Funzionali
*   **Inizializzazione:** Il sistema deve poter pre-allocare un pool di memoria contigua specificando la dimensione dei blocchi (`block_size`) e il numero massimo di blocchi (`block_count`).
*   **Allocazione ($O(1)$):** Deve restituire un puntatore a un blocco libero del pool in tempo costante. Se il pool è esaurito, deve restituire `NULL` (o lanciare un'eccezione in C++) oppure richiedere un nuovo blocco contiguo se configurato in modalità dinamica.
*   **Deallocazione ($O(1)$):** Deve contrassegnare un blocco precedentemente allocato come nuovamente disponibile in tempo costante, senza restituirlo al sistema operativo immediatamente.
*   **Thread Safety:** Il pool deve supportare accessi concorrenti da parte di più thread senza corruzione della memoria (facoltativo o configurabile tramite macro di compilazione per massimizzare le prestazioni single-thread).

---

## 3. Requisiti Non Funzionali
*   **Nessun Memory Leak:** Al momento della distruzione del pool, tutta la memoria pre-allocata deve essere restituita al sistema operativo.
*   **Overhead di Memoria Minimo:** L'uso di metadati interni per tracciare i blocchi liberi (es. tramite una lista concatenata interna o un bitmask) deve essere minimo.
*   **Compatibilità:** Scritto in ANSI C (o C++17) standard senza dipendenze esterne.

---

## 4. Architettura Logica & Algoritmo (Free List)
Il pool gestisce la memoria libera utilizzando una **Free List** implicita all'interno dei blocchi stessi. Quando un blocco è libero, i suoi primi byte vengono utilizzati per memorizzare un puntatore al blocco libero successivo. Questo azzera l'overhead di metadati per i blocchi non utilizzati.

```
+-------------------------------------------------------------------+
|                           Memory Pool                             |
+-------------------------------------------------------------------+
| [Blocco 1 (Libero)] -> Contiene puntatore al Blocco 2             |
| [Blocco 2 (Allocato)] -> Contiene dati utente                     |
| [Blocco 3 (Libero)] -> Contiene puntatore al Blocco 4             |
| [Blocco 4 (Libero)] -> Contiene NULL (Fine lista)                 |
+-------------------------------------------------------------------+
```

---

## 5. API / Interfaccia Pubblica (C)

```c
typedef struct memory_pool memory_pool_t;

// Inizializza il pool di memoria
memory_pool_t* memory_pool_create(size_t block_size, size_t block_count);

// Alloca un blocco dal pool (O(1))
void* memory_pool_alloc(memory_pool_t* pool);

// Rilascia un blocco nel pool (O(1))
void memory_pool_free(memory_pool_t* pool, void* block);

// Distrugge il pool rilasciando tutta la memoria al sistema operativo
void memory_pool_destroy(memory_pool_t* pool);
```

---

## 6. Strategia di Verifica e Test
1.  **Test di Correttezza:** Allocazione di tutti i blocchi fino all'esaurimento, verifica del comportamento con input nulli o puntatori esterni al pool.
2.  **Verifica Leak (Valgrind):**
    *   Comando di verifica:
        ```bash
        gcc -g -O0 test_pool.c memory_pool.c -o test_pool
        valgrind --leak-check=full --show-leak-kinds=all ./test_pool
        ```
    *   **Criterio di Successo:** `ERROR SUMMARY: 0 errors from 0 contexts`.
3.  **Benchmark prestazionale:** Confronto dei tempi di esecuzione tra `memory_pool_alloc`/`free` e `malloc`/`free` standard su un ciclo di 1.000.000 di iterazioni.
