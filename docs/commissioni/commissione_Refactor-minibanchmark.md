# Commissione Refactor minibanchmark — Benchmark OpenCL estendibile e portabile

| Campo | Valore |
|---|---|
| Nome | Refactor minibanchmark |
| Progetto | learn_OpenCL |
| Data apertura | 2026-10-03 |
| Stato | Pronta per lo sviluppo |
| Taglia | L (implementazione): struttura nuova per test con geometrie e argomenti propri (1D/2D/3D), parte comune portabile su Windows e Linux, otto test |
| Revisione | 9 |

> Un campo che non si applica va compilato con "N/A": un campo vuoto è ambiguo, "N/A" è una
> risposta. Quello che non è ancora deciso non si riempie con un'ipotesi: va in
> "Domande aperte e decisioni" in fondo al documento.

---

## Parte A — Comportamento atteso
*Cosa deve succedere dal punto di vista di chi usa il sistema, non come va realizzato.*

### A1. Contesto e motivazione
- **Esigenza / problema:** oggi minibanchmark è una sperimentazione: elenca piattaforme e device ed esegue solo il kernel `add` su ogni device, provando diverse dimensioni di work-group. Aggiungere un nuovo kernel di test richiede di toccare il codice in più punti, perché nome del kernel, buffer, argomenti e misurazione sono scritti a mano per `add`.
- **Per chi e perché ora:** per me, come esercizio di studio: voglio fissare uno schema pulito per testare device e piattaforme prima di implementare questo tipo di funzione altrove. Lo schema verrà poi portato su altri progetti.
- **Materiale di partenza:** il codice attuale di `minibanchmark/` (`minibanchmark.c`, `kernels/add.cl`).

### A2. Perimetro
- **Dentro:**
  - riordinare minibanchmark mantenendo ciò che fa oggi (elenco piattaforme/device, esecuzione dei test su ogni device);
  - poter aggiungere un nuovo test in modo semplice e localizzato: si aggiunge il file `.cl` e una sola registrazione del test, senza toccare il resto del programma;
  - supportare kernel con geometria e argomenti propri (1D, 2D, 3D, local memory);
  - funzionamento su Windows e Linux;
  - otto test funzionanti: `add`, `sub`, `mul`, `div` (1D elemento per elemento), `max` (riduzione: massimo dell'array), `mmul` (2D, memoria globale), `mmul_tiled` (2D, local memory a tile, solo forme quadrate), `stencil3d` (3D, stencil a 7 punti);
  - completare ciò che la descrizione del programma promette: a fine esecuzione indicare il device migliore e la configurazione migliore (dimensione del work-group) per ogni test;
  - verificare la correttezza del risultato di ogni kernel;
  - tempo massimo di esecuzione di un kernel configurabile;
  - in caso di fallimento di una piattaforma, un device o un test: segnalare e proseguire, mai crash;
  - la parte comune (scoperta piattaforme/device, esecuzione, misura, verifica, riepilogo) deve poter essere portata in un altro progetto senza i test di minibanchmark.
- **Fuori (esplicitamente escluso):**
  - output su file (CSV/JSON) e grafici: l'output resta su console;
  - parametri da riga di comando (scelta del device, dimensione dati, tempo massimo);
  - trasformare la parte comune in una libreria installabile o pacchettizzata: si porta copiando i file;
  - ottimizzazione dei kernel stessi.

### A3. Attori
| Attore | Ruolo / permessi | Cosa deve poter fare |
|---|---|---|
| Chi esegue il benchmark (io) | lancia l'eseguibile | vedere i device disponibili, i tempi per kernel e configurazione, e un riepilogo finale con device e configurazione migliori |
| Chi aggiunge un test (io) | modifica il sorgente | aggiungere un kernel di test in un solo punto, senza toccare la logica comune, anche in un altro progetto |

### A4. Scenari d'uso

**S1 — Esecuzione completa del benchmark**
- **Attore:** chi esegue
- **Situazione di partenza:** almeno una piattaforma OpenCL installata
- **Cosa fa l'attore (passi):**
  1. Lancia l'eseguibile.
- **Risultato atteso:** vede l'elenco di piattaforme e device con le caratteristiche principali; per ogni device, kernel e configurazione vede il tempo misurato e l'esito della verifica; alla fine vede un riepilogo con, per ogni test, il device e la configurazione migliori e, per ogni device, la configurazione migliore di ciascun test.

**S2 — Aggiungere un kernel 1D elemento per elemento**
- **Attore:** chi aggiunge un test
- **Situazione di partenza:** minibanchmark compila e funziona
- **Cosa fa l'attore (passi):**
  1. Scrive il file `.cl`.
  2. Registra il test in un solo punto (nome, file, come preparare gli input, come calcolare il risultato atteso).
  3. Ricompila.
- **Risultato atteso:** il nuovo test compare nell'esecuzione e nel riepilogo senza modifiche alla parte comune.

**S3 — Aggiungere un kernel con geometria propria**
- **Attore:** chi aggiunge un test
- **Situazione di partenza:** minibanchmark compila e funziona
- **Cosa fa l'attore (passi):**
  1. Scrive il file `.cl` (es. 2D, 3D o con local memory).
  2. Registra il test dichiarando dimensioni globali e argomenti; se la local memory dipende dalla dimensione del work-group, indica come calcolarla; se il kernel funziona solo con certe forme di work-group, dichiara il vincolo (es. "solo quadrate").
  3. Ricompila.
- **Risultato atteso:** come S2.

**S4 — Portare il meccanismo in un altro progetto**
- **Attore:** chi aggiunge un test, in un altro repository
- **Situazione di partenza:** un progetto che usa OpenCL
- **Cosa fa l'attore (passi):**
  1. Copia la parte comune.
  2. Registra i propri test.
- **Risultato atteso:** compila e funziona senza i kernel e i test di minibanchmark.

**S5 — Qualcosa fallisce su un device** (percorso negativo di S1)
- **Attore:** chi esegue
- **Situazione di partenza:** un test non compila, non è supportato, sbaglia il risultato o supera il tempo massimo su un device
- **Cosa fa l'attore (passi):**
  1. Lancia l'eseguibile.
- **Risultato atteso:** il problema è segnalato con il motivo, il test o la configurazione coinvolta vengono saltati, il resto dell'esecuzione prosegue; nel riepilogo compaiono solo misure valide.

### A5. Regole di business
- **RB-1:** ogni test registrato viene eseguito su ogni device di ogni piattaforma.
- **RB-2:** per ogni coppia test/device il benchmark genera automaticamente le configurazioni di work-group dalle caratteristiche del device e del kernel: parte dalla dimensione minima (multiplo preferito del kernel sul device) e prova tutti i multipli fino al massimo ammesso. In 2D e 3D prova le forme (X×Y, X×Y×Z) in cui ogni lato è una potenza di 2, il prodotto è un multiplo della dimensione minima e non supera il massimo, e che rispettano i limiti per dimensione del device. Il test non elenca configurazioni, ma può dichiarare un vincolo di forma (es. "solo quadrate") che il benchmark rispetta. Le configurazioni che richiedono più local memory di quella del device sono scartate; le configurazioni scartate sono riportate raggruppate per motivo, con il loro numero.
- **RB-3:** il tempo misurato è solo quello di esecuzione del kernel sul device (esclusi trasferimenti, compilazione, preparazione dei dati).
- **RB-4:** ogni configurazione viene eseguita 1 volta a vuoto e poi 5 volte misurate; il tempo riportato è la mediana.
- **RB-5:** una misura è valida solo se l'output corrisponde al risultato atteso calcolato sull'host (con tolleranza sui float); una misura con risultato errato è segnata **ERRATO** e non entra nel riepilogo.
- **RB-6:** ogni test dichiara quante operazioni in virgola mobile (FLOP) esegue per una data dimensione effettiva; per ogni misura si calcolano i FLOPS (FLOP / tempo). La configurazione migliore di un test su un device è quella con i FLOPS più alti tra le misure valide; il device migliore per un test è quello con la configurazione migliore a FLOPS più alti. Il riepilogo mostra anche tempo, dati elaborati e percentuale scartata.
- **RB-7:** se un test non può girare su un device (compilazione fallita, memoria insufficiente, kernel non creabile), viene saltato su quel device con il motivo e l'esecuzione continua.
- **RB-8:** aggiungere un test non richiede modifiche alla parte comune.
- **RB-9:** la parte comune non fa riferimento a nessun test specifico di minibanchmark.
- **RB-10:** ogni esecuzione di un kernel ha un tempo massimo: un valore predefinito globale di 10 s, sovrascrivibile per singolo test, entrambi configurati nel codice. Se un'esecuzione lo supera viene fermata: la configurazione è segnata **TIMEOUT**, si saltano le ripetizioni rimanenti e le configurazioni successive di quel test su quel device, e si passa al test successivo con il device di nuovo libero. Se il sistema operativo ferma il kernel prima del limite (es. TDR su GPU Windows), anche questo è **TIMEOUT**, con la nota "fermato dal sistema operativo". Le misure in TIMEOUT non entrano nel riepilogo.
- **RB-11:** la dimensione di riferimento dei dati è fissa e dichiarata dal test. Per ogni configurazione la dimensione effettiva è il multiplo più grande della dimensione del work-group (per ciascuna dimensione) che non supera il riferimento; i buffer sono creati e popolati con la dimensione effettiva, così nessun elemento resta senza calcolo e non si lanciano work-item in eccesso. Per ogni configurazione l'output mostra la quantità di dati elaborati e la percentuale scartata rispetto al riferimento. Se la dimensione effettiva non entra nella memoria del device, il test è saltato su quel device (vedi RB-7).
- **RB-12:** se durante un test su un device non arriva nessun segnale di avanzamento per 120 s, al di fuori delle esecuzioni del kernel (es. compilazione, preparazione dei dati, verifica bloccate), il test viene fermato su quel device con il motivo "il processo non risponde" e l'esecuzione prosegue con il test successivo.

### A6. Quando qualcosa va storto
| Situazione | Cosa vede / riceve l'utente | Cosa NON deve succedere |
|---|---|---|
| Nessuna piattaforma OpenCL | "Nessuna piattaforma OpenCL trovata", uscita con codice di errore | crash |
| Piattaforma senza device | segnalazione, si passa alla piattaforma successiva | interruzione del programma |
| File `.cl` mancante | "Test X: file non trovato `<path>`", test saltato su tutti i device | crash, salto dei test degli altri kernel |
| Compilazione fallita su un device | messaggio + log di compilazione del device, test saltato su quel device | interruzione degli altri test |
| Configurazione di work-group non supportata | configurazioni scartate riportate raggruppate per motivo, con il numero | tempo riportato come valido |
| Memoria insufficiente per i dati del test | test saltato su quel device con il motivo | crash, allocazioni parziali non liberate |
| Risultato errato | **ERRATO** + primo indice diverso con valore atteso e ottenuto | misura inclusa nel riepilogo |
| Esecuzione oltre il tempo massimo | **TIMEOUT** con il limite configurato (o "fermato dal sistema operativo") | programma o device bloccati, misura inclusa nel riepilogo |
| Test bloccato fuori dall'esecuzione del kernel (compilazione, preparazione, verifica) per 120 s | "il processo non risponde", test saltato su quel device | programma bloccato in attesa |
| Nessuna misura valida per un test | nel riepilogo: "nessun device valido" per quel test | riepilogo vuoto o con valori casuali |

### A7. Criteri di accettazione
- **CA-1** (S1, RB-1): **Dato** un sistema con almeno una piattaforma e un device **Quando** lancio il programma **Allora** vedo piattaforme e device con le loro caratteristiche e ogni test registrato viene eseguito su ogni device.
- **CA-2** (S1, RB-2): **Dato** un test 1D e un device con multiplo preferito 32 e massimo 256 **Quando** il test gira **Allora** vengono provate 32, 64, 96, 128, 160, 192, 224, 256.
- **CA-3** (S3, RB-2): **Dato** un test con local memory che per le configurazioni più grandi supera la local memory del device **Quando** il test gira **Allora** quelle configurazioni sono scartate e riportate raggruppate per motivo con il loro numero, e le altre vengono eseguite.
- **CA-17** (S3, RB-2): **Dato** un test 2D che dichiara il vincolo "solo quadrate" **Quando** il test gira **Allora** vengono provate solo forme quadrate (es. 8×8, 16×16) entro i limiti del device; senza vincolo vengono provate anche le forme rettangolari (es. 16×8, 8×16).
- **CA-4** (RB-3, RB-4): **Dato** un test in esecuzione **Quando** termina una configurazione **Allora** l'output mostra la mediana di 5 esecuzioni misurate, dichiarata come "solo esecuzione kernel".
- **CA-5** (S5, RB-5): **Dato** un kernel volutamente sbagliato (es. `add` che calcola `A-B`) **Quando** il test gira **Allora** vedo **ERRATO** con indice, valore atteso e valore ottenuto, e la misura non compare nel riepilogo.
- **CA-6** (S1, RB-6): **Dato** un'esecuzione completa **Quando** il programma termina **Allora** il riepilogo mostra per ogni test il device migliore con configurazione, FLOPS e tempo, e per ogni device la configurazione migliore di ciascun test (scelta per FLOPS).
- **CA-7** (S5, RB-7): **Dato** un `.cl` con un errore di sintassi **Quando** lancio il programma **Allora** vedo il log di compilazione, quel test è saltato e gli altri girano.
- **CA-8** (S5): **Dato** un file `.cl` mancante **Quando** lancio il programma **Allora** vedo "file non trovato" con il path e gli altri test girano.
- **CA-9** (A6): **Dato** un sistema senza piattaforme OpenCL **Quando** lancio il programma **Allora** vedo il messaggio e il programma esce con codice diverso da 0, senza crash.
- **CA-10** (S2, RB-8): **Dato** minibanchmark funzionante **Quando** aggiungo un nuovo test 1D **Allora** le modifiche riguardano solo il nuovo `.cl` e il punto di registrazione, e il test compare nell'esecuzione e nel riepilogo.
- **CA-11** (S3, RB-8): **Dato** il programma completo **Quando** lo eseguo **Allora** `max` (riduzione), `mmul` e `mmul_tiled` (2D) e `stencil3d` (3D) girano e superano la verifica, senza che la parte comune contenga codice specifico per loro.
- **CA-12** (S4, RB-9): **Dato** una cartella vuota **Quando** vi copio solo la parte comune e un test di prova **Allora** il programma compila e gira senza alcun file di minibanchmark.
- **CA-13** (RB-10): **Dato** un tempo massimo di 1 s e un kernel di prova che dura di più **Quando** il test gira **Allora** vedo **TIMEOUT**, le configurazioni rimanenti di quel test su quel device sono saltate, il programma passa al test successivo, il test successivo sullo stesso device viene eseguito regolarmente e la misura non compare nel riepilogo.
- **CA-14** (S5, RB-7, RB-11): **Dato** un test con dati oltre la memoria allocabile del device **Quando** il test gira **Allora** è saltato su quel device con il motivo.
- **CA-15** (S5, RB-6): **Dato** un test senza alcuna misura valida **Quando** il programma termina **Allora** il riepilogo riporta "nessun device valido" per quel test.
- **CA-16** (A2): **Dato** il programma completo **Quando** lo eseguo sulla mia macchina **Allora** tutti e otto i test (`add`, `sub`, `mul`, `div`, `max`, `mmul`, `mmul_tiled`, `stencil3d`) superano la verifica su almeno un device.
- **CA-18** (RB-11): **Dato** un test 1D con riferimento 16.777.216 elementi **Quando** gira con work-group 96 **Allora** l'output mostra 16.777.152 elementi elaborati e 0,00038% scartati, e la verifica copre tutti i 16.777.152 elementi.
- **CA-19** (S3, RB-2): **Dato** il test 3D `stencil3d` **Quando** gira **Allora** vengono provate forme X×Y×Z con lati potenze di 2, il cui prodotto è multiplo della dimensione minima, entro il massimo e i limiti per dimensione del device.
- **CA-21** (RB-12): **Dato** un test di prova che si blocca nella preparazione dei dati **Quando** il programma gira **Allora** dopo 120 s vedo "il processo non risponde", il test è saltato su quel device e l'esecuzione prosegue.
- **CA-20** (A2): **Dato** il programma completo **Quando** lo compilo ed eseguo su Windows e su Linux **Allora** funziona su entrambi, incluso il timeout (CA-13).

### A8. Vincoli non tecnici
- **Scadenza / priorità:** nessuna scadenza; esercizio di studio personale.
- **Utenti o clienti coinvolti:** solo io.
- **Dati personali o riservati coinvolti?** no.
- **Abitudini degli utenti da non rompere:** l'output resta su console; si compila con un comando `gcc` analogo a quello del README (aggiornato in un solo punto se i file diventano più di uno); si lancia dalla radice del repository come oggi.

### A9. Appunti per la Parte B
- Test descritti come struttura "registrabile" (file, nome, geometria, configurazioni, preparazione input, argomenti, verifica).
- Misura con eventi OpenCL (profiling) invece di `clock()`.
- In OpenCL un kernel avviato non si può interrompere: il timeout va gestito controllando lo stato dell'evento e smettendo di attendere; il device resta occupato finché il kernel non termina. Su GPU Windows il TDR (circa 2 s di default) può intervenire prima del limite.
- Timeout: ogni coppia test/device gira in un processo figlio che il padre termina al superamento del limite (unico modo per fermare davvero il kernel). Riconoscimento del sistema operativo con le macro del preprocessore `_WIN32`/`_WIN64`, `__linux__` (`__APPLE__` non supportato) per scegliere l'implementazione dei processi, dentro la parte comune (`bench_proc.c`), così il `main` resta uguale in ogni progetto.
- Problemi visti nel codice attuale da sistemare nel refactor: `free(kernel_name)` su stringa letterale; `buffer_size` passato come `size_t` mentre il kernel legge `unsigned int*`; `CL_DEVICE_MAX_WORK_GROUP_SIZE` letto come `cl_ulong` invece di `size_t`; `;` finale in `KERNEL_SOURCE_PATH`; context, queue e program mai rilasciati.

---

## Parte B — Specifica tecnica
*Non riscrive la Parte A: le incongruenze trovate vanno in "Domande aperte e decisioni".*

**Legenda:** **[P]** = prescritta (deciso ora; durante l'implementazione non si cambia senza
aggiornare la commissione) · **[D]** = a discrezione (si decide implementando, annotando la scelta).

**Taglia tecnica:** L (confermata): parte comune nuova, otto test con geometrie diverse, timeout tramite processi figli su due sistemi operativi.

### B0. Alternative valutate
Struttura dei file comune a entrambe: parte comune portabile (`bench/`), test (`tests/`), `main.c`. Le alternative differiscono su come un test descrive se stesso.

| Alternativa | Pro | Contro | Scelta? |
|---|---|---|---|
| A — Descrittore + funzioni del test (`setup`, `set_args`, `verify`, `teardown`, …) con helper comuni | Regge qualunque kernel (riduzioni, local memory variabile, 2D/3D); C semplice, facile da portare | Un test richiede qualche funzione breve (mitigato da helper e dalla famiglia 1D generica) | **Sì** |
| B — Test descritti solo come dati (descrittori degli argomenti, generatore, riferimento) | Test 1D minimi | Rigido: riduzione e `mmul` richiedono casi speciali, il linguaggio dei descrittori cresce; peggiore per la portabilità (S4) | No |

Motivo: S3 e S4 pesano più della comodità sui test 1D; gli helper recuperano gran parte di quella comodità.

### B1. Impatto
- **Componenti coinvolti:** programma host C, kernel OpenCL, API di sistema per i processi (Windows, Linux). Nessun DB, nessuna integrazione esterna.
- **Moduli / file principali da toccare:**

  | File | Contenuto | P/D |
  |---|---|---|
  | `minibanchmark/bench/bench.h` | interfaccia pubblica della parte comune | [P] |
  | `minibanchmark/bench/bench.c` | F1, F3, F4, F6 | [P] |
  | `minibanchmark/bench/bench_proc.c` | F5: processi, timeout, riconoscimento del sistema operativo | [P] |
  | `minibanchmark/bench/bench_elementwise.c` | famiglia generica 1D elemento per elemento (F7) | [P] |
  | `minibanchmark/tests/tests.h`, `tests.c` | registro dei test, descrittori e `op_*` della famiglia 1D | [P] |
  | `minibanchmark/tests/test_max.c`, `test_mmul.c`, `test_stencil3d.c`, `test_selftest.c` | test non 1D e test di prova | [D] nomi |
  | `minibanchmark/kernels/*.cl` | `add`, `sub`, `mul`, `div`, `max`, `mmul`, `mmul_tiled`, `stencil3d` + kernel di prova | [D] nomi |
  | `minibanchmark/main.c` | punto di ingresso (padre o figlio) | [P] |
  | `minibanchmark/minibanchmark.c` | **eliminato** | [P] |
  | `README.md` | sezione "minibanchmark" con i comandi di compilazione | [P] |
- **Modifiche al DB (schema):** nessuna.
- **Tocca configurazione o deploy?** `Sì`: cambia il comando di compilazione (più sorgenti, `-std=c11 -O2 -lm`). Il figlio è lo stesso eseguibile rilanciato con un argomento interno `--bench-child <test> <piattaforma> <device>`, non documentato come opzione per l'utente.

### B2. Vincoli architetturali
- [P] C11 (`-std=c11`), con controllo `#if __STDC_VERSION__ < 201112L` / `#error` in `bench.h`.
- [P] `CL_TARGET_OPENCL_VERSION 300`; la versione di ogni device si legge a runtime: ≥ 2.0 → `clCreateCommandQueueWithProperties`, 1.x → `clCreateCommandQueue` (con `CL_USE_DEPRECATED_OPENCL_1_2_APIS` definita prima di `#include <CL/cl.h>`).
- [P] I kernel si compilano senza `-cl-std` (ogni device usa la sua OpenCL C 1.x più alta) e usano solo funzionalità OpenCL C 1.2.
- [P] Dipendenze a senso unico: `tests/` → `bench.h`; `main.c` → `bench.h` + `tests.h`. `bench/` non include nulla di `tests/` e non contiene percorsi di minibanchmark (RB-9).
- [P] Codice specifico del sistema operativo solo in `bench_proc.c`, dentro `#if defined(_WIN32) || defined(_WIN64)` / `#elif defined(__linux__)` / `#else #error "sistema operativo non supportato"`.
- [P] Ogni chiamata OpenCL è controllata; l'errore riporta nome della chiamata e codice; nessuna `exit` nella parte comune: l'errore risale al chiamante.
- [P] Ogni risorsa OpenCL e ogni allocazione è rilasciata su tutti i percorsi, compresi quelli di errore.
- [P] Risultati su `stdout`, errori e avvisi su `stderr`.
- [P] Nessuna dipendenza oltre OpenCL, libreria standard C e API di sistema per i processi.
- [P] Commenti Doxygen sulle funzioni pubbliche di `bench.h`.
- [D] Nessuno stato globale modificabile nella parte comune, salvo necessità annotate.

### B3. Copertura della Parte A
| Riferimento Parte A | Coperto da | Note |
|---|---|---|
| RB-1 | F1, F5 | il padre esegue ogni coppia test/device |
| RB-2 | F3, F8, F9 | `local_mem_bytes`; `EQUAL` in `mmul_tiled` |
| RB-3 | F4 | intervallo di profiling del kernel |
| RB-4 | F4 | 1 + 5 esecuzioni, mediana |
| RB-5 | F2, F4, F7, F8, F9, F10 | `bench_compare_float` + verifica di ogni test |
| RB-6 | F2, F6 | `flop()` obbligatoria; classifica per FLOPS |
| RB-7 | F1, F4 | device esclusi; memoria insufficiente |
| RB-8 | F2, F7 | registro unico in `tests.c` |
| RB-9 | F2, F5, F11 | `bench/` non conosce i test; `bench_main` |
| RB-10 | F4, F5 | figlio terminato; `TIMEOUT_OS` |
| RB-11 | F3, F4, F7, F8, F9, F10 | dimensione effettiva e % scartata |
| RB-12 | F5 | controllo di inattività 120 s |
| CA-1 | F1, F5 | |
| CA-2 | F3 | |
| CA-3 | F3, F11 | `selftest_localmem` |
| CA-4 | F4 | |
| CA-5 | F4, F7 | variante errata di `add.cl` |
| CA-6 | F6 | |
| CA-7 | F4 | |
| CA-8 | F4 | controllo nel padre |
| CA-9 | F1 | non verificato in questa commissione (Q-21) |
| CA-10 | F2, F7 | test temporaneo `rsub` + `git diff` |
| CA-11 | F8, F9, F10 | |
| CA-12 | F2, F11 | |
| CA-13 | F5, F11 | `selftest_timeout` |
| CA-14 | F4 | |
| CA-15 | F6 | |
| CA-16 | F7, F8, F9, F10 | |
| CA-17 | F3, F9 | `mmul_tiled` |
| CA-18 | F3, F7 | |
| CA-19 | F3, F10 | |
| CA-20 | F5 | verifica rimandata (Q-18) |
| CA-21 | F5, F11 | `selftest_hang` |

---

### F1 (BE): Scoperta piattaforme e device
**Riferimenti Parte A:** S1, RB-1, RB-7, CA-1, CA-9, A6 (nessuna piattaforma, piattaforma senza device)

#### Input
**Parametri / variabili in entrata:** nessuno.

**Dati letti da OpenCL**
| Origine | Cosa | P/D |
|---|---|---|
| `clGetPlatformIDs` | tutte le piattaforme | [P] |
| `clGetDeviceIDs(CL_DEVICE_TYPE_ALL)` | tutti i device di ogni piattaforma | [P] |
| `clGetPlatformInfo` | nome, vendor, versione | [P] |
| `clGetDeviceInfo` (stampa) | nome, tipo, vendor, versione driver, versione OpenCL, versione OpenCL C, compute unit, frequenza, risoluzione del timer di profiling | [P] elenco, [D] formato |
| `clGetDeviceInfo` (limiti per F3/F4) | `MAX_WORK_GROUP_SIZE` (`size_t`), `MAX_WORK_ITEM_DIMENSIONS`, `MAX_WORK_ITEM_SIZES[]`, `LOCAL_MEM_SIZE`, `GLOBAL_MEM_SIZE`, `MAX_MEM_ALLOC_SIZE`, `AVAILABLE`, `COMPILER_AVAILABLE` | [P] |

**Config letta:** nessuna.

#### Output atteso
**Caso normale:** su `stdout`, prima dei test, elenco di piattaforme e device con le caratteristiche (stile di `src/platform_info.c`, memorie in MB/KB) [D]. In memoria un array di `bench_device` con indici di piattaforma e device, etichetta "piattaforma / device", versione OpenCL interpretata, limiti [P].

Il figlio riceve gli indici di piattaforma e device, rifà la scoperta e controlla che il nome corrisponda; se no, errore e coppia saltata [P].

**Casi di errore**
| Condizione | Codice / eccezione | Cosa restituisce | P/D |
|---|---|---|---|
| 0 piattaforme o `CL_PLATFORM_NOT_FOUND_KHR` | errore | "Nessuna piattaforma OpenCL trovata" su `stderr`; `main` esce con 1 | [P] |
| Piattaforma con 0 device (`CL_DEVICE_NOT_FOUND`) | avviso | "Piattaforma X: nessun device", si continua | [P] |
| Nessun device utilizzabile in totale | errore | messaggio, uscita con 1 | [P] |
| `AVAILABLE` o `COMPILER_AVAILABLE` falsi | — | device mostrato con "escluso: motivo", nessun test su di esso | [P] |
| Errore su un dato solo da stampare | — | "n/d", si continua | [P] |
| Errore su un limite usato da F3/F4 | — | device escluso con il motivo | [P] |
| Versione non interpretabile | avviso | device trattato come 1.x | [P] |

**Dati scritti/aggiornati sul DB:** N/A.

#### Regole di business applicate
- RB-1: tutte le piattaforme e tutti i tipi di device, salvo esclusi con motivo.
- RB-7: device non utilizzabili esclusi con motivo.

#### Casi limite e assunzioni
- **Casi limite da gestire:** stessa GPU su due piattaforme = due device con etichette distinte; `MAX_WORK_ITEM_DIMENSIONS` < 3 (F3 salta i test 3D); `malloc` fallita.
- **Assunzioni / casi esplicitamente non gestiti:** ordine di piattaforme e device stabile tra padre e figlio (il controllo sul nome copre il contrario).

#### Comportamento su fallimento
- Errori come codice negativo, messaggio su `stderr`, memoria liberata. Nessuna transazione.

#### Come
- [P] `bench_discover(bench_device **out, size_t *n)`, `bench_devices_free()`; stampa in una funzione separata (il figlio scopre senza stampare).
- [P] Correzioni rispetto al codice attuale: `ADDRESS_BITS` è `cl_uint`, `MAX_WORK_GROUP_SIZE` è `size_t`, dimensioni delle stringhe controllate.
- [D] Funzioni di stampa per tipo riprese da `src/platform_info.c`.

#### Test
- CA-1: sulla macchina di sviluppo 2 piattaforme e 3 device (AMD gfx1032; OpenCLOn12 RX 6600 XT e Microsoft Basic Render Driver).
- CA-9: non verificato (Q-21).

---

### F2 (BE): Modello di test, registro e helper
**Riferimenti Parte A:** S2, S3, S4, RB-5, RB-6, RB-8, RB-9, RB-11, CA-10, CA-12

#### Input
**Parametri / variabili in entrata** — descrittore `bench_test` in `bench.h`:
| Nome | Tipo | Obbligatorio | Vincoli / validazione richiesta | P/D |
|---|---|---|---|---|
| `name` | `const char*` | sì | non vuoto, unico nel registro | [P] |
| `source_path` | `const char*` | sì | percorso del `.cl` relativo alla cartella di lancio | [P] |
| `kernel_name` | `const char*` | sì | non vuoto | [P] |
| `build_options` | `const char*` | no | `NULL` = nessuna | [P] |
| `work_dim` | `cl_uint` | sì | 1, 2 o 3 | [P] |
| `ref_size[3]` | `size_t` | sì | > 0 per le prime `work_dim` dimensioni | [P] |
| `shape` | enum | no | `BENCH_SHAPE_ANY` (predefinito) / `BENCH_SHAPE_EQUAL` | [P] |
| `timeout_s` | `double` | no | 0 = valore globale (10 s), altrimenti > 0 | [P] |
| `user` | `const void*` | no | dati statici del test | [P] |
| `setup(run)` | funzione | sì | crea e popola i buffer per `run->eff_size` | [P] |
| `set_args(run)` | funzione | sì | argomenti, local memory da `run->local_size` | [P] |
| `local_mem_bytes(local)` | funzione | no | `NULL` se non usa local memory | [P] |
| `verify(run, &mismatch)` | funzione | sì | 0 corretto, 1 ERRATO (compila `mismatch`), < 0 errore | [P] |
| `flop(eff_size)` | funzione | sì | FLOP per la dimensione effettiva | [P] |
| `teardown(run)` | funzione | sì | chiamata sempre, tollera risorse `NULL` | [P] |

`bench_run` passato alle funzioni: in sola lettura `context`, `queue`, `kernel`, `device`, `work_dim`, `eff_size[3]`, `local_size[3]`; `void *state` di proprietà del test [P]. Struttura pubblica o opaca con accessori [D].

Registro `tests/tests.c`: `const bench_test *const BENCH_TESTS[]` + conteggio, unico punto da modificare per aggiungere un test [P]. Opzioni globali `bench_options` (`BENCH_OPTIONS_DEFAULT`: timeout 10 s, 1 esecuzione a vuoto, 5 misurate, inattività 120 s) [P].

**Dati letti dal DB:** N/A. **Config letta:** nessuna.

#### Output atteso
**Caso normale:** `bench_main(argc, argv, tests, n, &options)` esegue tutto (F1–F6) e restituisce il codice di uscita [P].

Helper della parte comune [P] presenza, [D] firme: `bench_buffer_create`, `bench_buffer_read` (bloccante), `bench_fill_random_float` (seed fisso), `bench_compare_float(expected, got, n, rel_tol, abs_tol, &mismatch)` (primo indice diverso, valore atteso e ottenuto).

**Casi di errore**
| Condizione | Codice / eccezione | Cosa restituisce | P/D |
|---|---|---|---|
| Descrittore non valido (campo obbligatorio mancante, `work_dim` fuori 1–3, `ref_size` 0, nome duplicato) | — | all'avvio, prima di qualsiasi test: messaggio con nome del test e campo; test escluso, gli altri girano | [P] |
| Registro vuoto | — | "nessun test registrato", uscita 1 | [P] |
| `setup` fallisce per memoria | — | RB-7: test saltato su quel device, `teardown` comunque chiamata | [P] |
| `verify` < 0 | — | configurazione non valida, fuori dal riepilogo | [P] |

**Dati scritti/aggiornati sul DB:** N/A.

#### Regole di business applicate
- RB-8: nuovo test = `.cl` + descrittore + riga nel registro.
- RB-9: `bench.h` non conosce alcun test.
- RB-11: `run->eff_size` arriva già calcolata.
- RB-6: `flop()` obbligatoria.

#### Casi limite e assunzioni
- **Casi limite da gestire:** `teardown` dopo `setup` parziale; `local_mem_bytes` con `work_dim` = 1.
- **Assunzioni / casi esplicitamente non gestiti:** test statici (compilati), nessuna registrazione a runtime; verifica sull'host.

#### Comportamento su fallimento
- Errori come codice negativo, registrati su `stderr` una volta sola dove avvengono. Nessuna transazione.

#### Come
- [P] `bench_test`, `bench_run`, `bench_options` in `bench.h`; validazione dei descrittori in `bench.c` all'avvio.
- [P] Famiglia generica "elemento per elemento" in `bench/bench_elementwise.c` (vedi F7).

#### Test
- Descrittore non valido registrato temporaneamente → messaggio e altri test eseguiti.
- CA-10 (vedi F7), CA-12 (vedi F11).
- [D] Test unitario di `bench_compare_float`.

---

### F3 (BE): Generazione delle configurazioni
**Riferimenti Parte A:** S3, RB-2, RB-11, CA-2, CA-3, CA-17, CA-18, CA-19

#### Input
**Parametri / variabili in entrata**
| Nome | Tipo | Obbligatorio | Vincoli / validazione richiesta | P/D |
|---|---|---|---|---|
| `work_dim`, `ref_size[]`, `shape`, `local_mem_bytes()` | da `bench_test` | sì | già validati (F2) | [P] |
| `MAX_WORK_ITEM_SIZES[]`, `MAX_WORK_ITEM_DIMENSIONS`, `LOCAL_MEM_SIZE` | da `bench_device` | sì | | [P] |
| massimo del kernel | `CL_KERNEL_WORK_GROUP_SIZE` | sì | | [P] |
| dimensione minima p | `CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE` | sì | lettura fallita o 0 → p = 1 con avviso | [P] |
| local memory statica | `CL_KERNEL_LOCAL_MEM_SIZE` | sì | | [P] |

F3 gira nel figlio, dopo il build (F4) [P].

**Dati letti dal DB:** N/A. **Config letta:** nessuna.

#### Output atteso
**Caso normale:** elenco di `bench_config { local[3], eff[3], discarded_pct }` [P]:
- 1D: L ∈ {p, 2p, 3p, …} con L ≤ massimo del kernel, ≤ `MAX_WORK_ITEM_SIZES[0]`, ≤ `ref_size[0]`.
- 2D/3D: tuple con **ogni lato potenza di 2**, ≤ `MAX_WORK_ITEM_SIZES[d]` e ≤ `ref_size[d]`, prodotto multiplo di p e ≤ massimo del kernel; con `EQUAL` tutti i lati uguali (in 1D ignorato).
- `eff[d] = floor(ref[d] / local[d]) · local[d]`; `discarded_pct = 1 − Π eff / Π ref`.
- Ordine: prodotto crescente, poi X, Y, Z [D].
- Su `stdout` (tramite il padre): "N configurazioni generate, M scartate" e le scartate **raggruppate per motivo** con il numero [P]; formato [D].

**Casi di errore**
| Condizione | Codice / eccezione | Cosa restituisce | P/D |
|---|---|---|---|
| `local_mem_bytes(local)` + statica > `LOCAL_MEM_SIZE` | — | configurazione scartata, motivo "local memory insufficiente" (raggruppato) | [P] |
| `work_dim` > `MAX_WORK_ITEM_DIMENSIONS` | — | test saltato su quel device con motivo | [P] |
| Nessuna configurazione valida | — | test saltato: "nessuna configurazione valida" | [P] |
| Lettura dei limiti del kernel fallita (escluso p) | codice OpenCL | test saltato con nome della chiamata e codice | [P] |

**Dati scritti/aggiornati sul DB:** N/A.

#### Regole di business applicate
- RB-2: configurazioni automatiche, vincolo `EQUAL` rispettato, scarti raggruppati.
- RB-11: dimensione effettiva e % scartata calcolate qui.

#### Casi limite e assunzioni
- **Casi limite da gestire:** `EQUAL` in 3D con una sola forma (gfx1032: 4×4×4); `ref_size[d]` < p in 1D → nessuna configurazione.
- **Assunzioni / casi esplicitamente non gestiti:** la dimensione globale lanciata è sempre `eff`, multipla di `local`; nessun uso dei work-group non uniformi di OpenCL 2.0.

#### Comportamento su fallimento
- Funzione pura salvo `clGetKernelWorkGroupInfo`; errori come codice; elenco liberato dal chiamante.

#### Come
- [P] `bench_configs_generate(test, device, kernel, &configs, &n)` in `bench.c`, cicli annidati sulle potenze di 2 con controllo del prodotto.

#### Test
- CA-2 (p = 32, massimo 256 → 32…256), CA-18, CA-3 (con `selftest_localmem`), CA-17, CA-19.
- [D] Test unitario di `bench_configs_generate` con limiti finti.

---

### F4 (BE): Esecuzione e misura (processo figlio)
**Riferimenti Parte A:** S1, S5, RB-3, RB-4, RB-5, RB-7, RB-10, RB-11, CA-4, CA-5, CA-7, CA-8, CA-14, CA-18

#### Input
**Parametri / variabili in entrata**
| Nome | Tipo | Obbligatorio | Vincoli / validazione richiesta | P/D |
|---|---|---|---|---|
| indice test, piattaforma, device | interi da `--bench-child` | sì | fuori intervallo → errore, uscita | [P] |
| `bench_options` | struttura | sì | 1 a vuoto + 5 misurate | [P] |
| sorgente `.cl` | file `test->source_path` | sì | | [P] |

**Dati letti dal DB:** N/A. **Config letta:** nessuna.

#### Output atteso
**Caso normale** — flusso per una coppia test/device [P]:
1. Scoperta e selezione del device per indice, controllo del nome (F1).
2. Context con un solo device; queue con `CL_QUEUE_PROFILING_ENABLE` (API scelta dalla versione, B2).
3. Build del program solo per quel device con `build_options`.
4. Kernel e configurazioni (F3).
5. Per ogni configurazione: `setup` (buffer ricreati, la dimensione effettiva cambia) e `set_args`; 1 esecuzione a vuoto + 5 misurate, ciascuna `clEnqueueNDRangeKernel` con evento + `clWaitForEvents`, tempo = `COMMAND_END − COMMAND_START`; mediana; messaggi `RUN_START`/`RUN_END` al padre attorno a ogni esecuzione; verifica sull'output dell'ultima esecuzione; FLOPS = `flop(eff) / mediana`; `teardown` sempre; messaggio `RESULT`.
6. Rilascio di kernel, program, queue, context; codice di uscita del figlio.

Il controllo del `.cl` mancante lo fa il padre una sola volta prima di lanciare i figli: un messaggio, test saltato su tutti i device (CA-8) [P].

Una riga su `stdout` per configurazione, stampata dal padre (es. `[AMD / gfx1032] add  L=96  dati 16.777.152/16.777.216 (scartati 0,00038%)  mediana 0,121 ms  8,26 GFLOPS  OK`) [D] formato. Il figlio non stampa mai su `stdout` se non tramite il protocollo [P].

**Casi di errore**
| Condizione | Codice / eccezione | Cosa restituisce | P/D |
|---|---|---|---|
| Build fallito | codice OpenCL | log `CL_PROGRAM_BUILD_LOG` completo, test saltato su quel device (CA-7) | [P] |
| Memoria (`CL_INVALID_BUFFER_SIZE`, `CL_MEM_OBJECT_ALLOCATION_FAILURE`, `CL_OUT_OF_HOST_MEMORY`) in `setup` o all'enqueue | codice OpenCL | "memoria insufficiente", test saltato su quel device (RB-7, CA-14) | [P] |
| `clEnqueueNDRangeKernel` rifiuta la configurazione | codice OpenCL | solo quella configurazione scartata con il codice | [P] |
| Errore durante l'esecuzione (stato evento negativo, device perso) | — | `TIMEOUT_OS`: **TIMEOUT "fermato dal sistema operativo"**, configurazioni rimanenti saltate, figlio esce | [P] |
| Verifica ERRATO | — | ERRATO con indice, atteso, ottenuto; fuori dal riepilogo; si prosegue | [P] |
| `set_args` / `verify` < 0 | — | test saltato su quel device con messaggio | [P] |

**Dati scritti/aggiornati sul DB:** N/A.

#### Regole di business applicate
- RB-3: solo l'intervallo di profiling del kernel. RB-4: 1 + 5, mediana. RB-5: verifica a ogni configurazione. RB-7: memoria → saltato. RB-10: errore in esecuzione = TIMEOUT. RB-11: buffer della dimensione effettiva.

#### Casi limite e assunzioni
- **Casi limite da gestire:** kernel vicini alla risoluzione del timer (mediana stampata comunque); `teardown` dopo `setup` parziale.
- **Assunzioni / casi esplicitamente non gestiti:** kernel idempotenti; output dell'ultima esecuzione rappresentativo.

#### Comportamento su fallimento
- Nessuna `exit` nella parte comune salvo il ritorno dal `main` del figlio; rilascio in ordine inverso con un'unica uscita di pulizia [D].

#### Come
- [P] `bench_child_run(test_idx, platform_idx, device_idx)`, `bench_measure_config()`, messaggi tramite un'unica famiglia `bench_report_*()` che scrive sul canale di F5 con `fflush` dopo ogni riga.
- [D] Mediana per ordinamento per inserzione.

#### Test
- CA-4, CA-5 (variante `A-B` di `add.cl`), CA-7 (errore di sintassi), CA-8 (`.cl` rinominato), CA-14 (`ref_size` oltre 6,7 GB), CA-18.

---

### F5 (BE): Supervisione a processi e timeout
**Riferimenti Parte A:** S1, S5, RB-1, RB-9, RB-10, RB-12, CA-13, CA-20, CA-21

#### Input
**Parametri / variabili in entrata**
| Nome | Tipo | Obbligatorio | Vincoli / validazione richiesta | P/D |
|---|---|---|---|---|
| `argc`/`argv` | — | sì | `argv[1] == "--bench-child"` → figlio, altrimenti padre | [P] |
| argomenti interni | `--bench-child <test> <piattaforma> <device>` | nel figlio | interi validati | [P] |
| tempo massimo | `test->timeout_s` o `options.timeout_s` (10 s) | sì | > 0 | [P] |
| inattività | `options.inactivity_s` (120 s) | sì | > 0 | [P] |
| percorso dell'eseguibile | Windows `GetModuleFileNameA(NULL)`; Linux `readlink("/proc/self/exe")` | sì | | [P] |

**Dati letti dal DB:** N/A. **Config letta:** nessuna.

#### Output atteso
**Caso normale:** il padre esegue F1 e stampa, valida i test (F2), controlla i `.cl`; per ogni test e device non escluso lancia un figlio con `stdout` su pipe (`stderr` sulla console); legge i messaggi riga per riga con attesa a tempo; raccoglie i risultati per F6 e stampa le righe di F4 [P]. Il figlio esegue F4 e termina con `DONE` e codice 0.

Protocollo testuale, un messaggio per riga, campi separati da tab, numeri in formato C con punto decimale (la formattazione all'italiana la fa il padre) [P] tipi, [D] campi esatti:
| Messaggio | Significato |
|---|---|
| `CONFIGS <generate> <scartate>` | esito di F3 |
| `DISCARD <numero> <motivo>` | scartate raggruppate per motivo |
| `RUN_START <cfg>` / `RUN_END <cfg>` | inizio/fine di ogni esecuzione (base del timeout) |
| `RESULT <local> <eff> <%scartata> <mediana_ns> <flop> <OK\|ERRATO> [indice atteso ottenuto]` | risultato di una configurazione |
| `TIMEOUT_OS <cfg>` | errore durante l'esecuzione, conta come TIMEOUT |
| `SKIP <motivo>` | test saltato su questo device |
| `DONE` | fine regolare |

**Casi di errore**
| Condizione | Codice / eccezione | Cosa restituisce | P/D |
|---|---|---|---|
| Nessun `RUN_END` entro il tempo massimo | — | figlio terminato, TIMEOUT con il limite, configurazioni rimanenti saltate, si passa al test successivo | [P] |
| Nessun messaggio per 120 s fuori dalle esecuzioni | — | figlio terminato, "il processo non risponde", test saltato su quel device (RB-12) | [P] |
| Figlio termina senza `DONE` | codice di uscita | "processo terminato in modo anomalo (codice X)", test saltato su quel device | [P] |
| Creazione di figlio o pipe fallita | codice di sistema | errore con chiamata e codice, coppia saltata | [P] |
| Riga non riconosciuta | — | avviso su `stderr`, riga ignorata | [P] |

**Dati scritti/aggiornati sul DB:** N/A.

#### Regole di business applicate
- RB-10: il kernel è fermato terminando il processo; il test successivo parte in un processo nuovo con il device libero.
- RB-12: controllo di inattività.
- RB-1: tutte le coppie test/device.

#### Casi limite e assunzioni
- **Casi limite da gestire:** percorso dell'eseguibile con spazi (virgolette nella riga di comando Windows); su Windows solo l'estremità di scrittura della pipe ereditabile; Ctrl+C chiude padre e figli insieme.
- **Assunzioni / casi esplicitamente non gestiti:** su Linux `fork` seguito subito da `execv` (il padre ha già inizializzato OpenCL: `fork` senza `exec` non è sicuro); cartella di lavoro ereditata; macOS non supportato.

#### Comportamento su fallimento
- Il padre non esce mai per l'errore di un figlio; ogni errore riguarda solo la coppia coinvolta.

#### Come
- [P] Interfaccia `bench_proc_spawn`, `bench_proc_read_line(timeout)`, `bench_proc_kill`, `bench_proc_wait` in `bench_proc.c`:
  - Windows: `CreatePipe`, `CreateProcessA`, `PeekNamedPipe` (attesa a intervalli), `TerminateProcess`, `QueryPerformanceCounter`;
  - Linux: `pipe`, `fork` + `execv`, `poll`, `kill(SIGKILL)`, `waitpid`, `clock_gettime(CLOCK_MONOTONIC)`;
  - altrimenti `#error`.
- [P] `bench_main` decide padre/figlio; `main.c` resta identico in ogni progetto.
- [D] Intervallo di attesa su Windows (5–10 ms).

#### Test
- CA-13 con `selftest_timeout` (limite 1 s, sotto il TDR), verificando che il test successivo sullo stesso device giri.
- CA-21 con `selftest_hang`; crash di un figlio con `selftest_crash`.
- CA-20 rimandato (Q-18).

---

### F6 (BE): Riepilogo
**Riferimenti Parte A:** S1, S5, RB-5, RB-6, RB-10, CA-6, CA-15

#### Input
**Parametri / variabili in entrata**
| Nome | Tipo | Obbligatorio | Vincoli / validazione richiesta | P/D |
|---|---|---|---|---|
| risultati raccolti dal padre | array dinamico | sì | per coppia: configurazioni con esito, mediana, FLOPS, eff, % scartata; scarti per motivo; motivo di salto | [P] |
| elenco device | da F1 | sì | etichette, esclusi | [P] |
| test esclusi in validazione | da F2 | sì | nome e motivo | [P] |

**Dati letti dal DB:** N/A. **Config letta:** nessuna.

#### Output atteso
**Caso normale** su `stdout` a fine esecuzione [P]:
1. **Per test** (ordine del registro): classifica di tutti i device per FLOPS della loro configurazione migliore, il primo marcato come migliore; per ciascuno configurazione, FLOPS, mediana, dati elaborati, % scartata.
2. **Per device:** per ogni test la configurazione migliore con FLOPS e tempo, oppure il motivo (saltato: motivo / TIMEOUT / solo ERRATO); device esclusi come "escluso: motivo".
3. **Conteggi:** configurazioni misurate, OK, ERRATO, TIMEOUT, scartate; coppie test/device saltate.

Codice di uscita: 0 se l'esecuzione è arrivata in fondo (anche con ERRATO/TIMEOUT); 1 solo per errori fatali (nessuna piattaforma, nessun device utilizzabile, nessun test valido) [P].

**Casi di errore**
| Condizione | Codice / eccezione | Cosa restituisce | P/D |
|---|---|---|---|
| Nessuna misura valida per un test | — | "nessun device valido" nella sezione 1 (CA-15) | [P] |
| Device senza misure valide | — | presente nella sezione 2 con i motivi | [P] |
| Allocazione fallita in raccolta | — | riepilogo parziale con "riepilogo incompleto" | [P] |

**Dati scritti/aggiornati sul DB:** N/A.

#### Regole di business applicate
- RB-6: ordinamento per FLOPS, con tempo, dati elaborati e % scartata visibili.
- RB-5, RB-10: ERRATO e TIMEOUT esclusi dalla scelta del migliore.

#### Casi limite e assunzioni
- **Casi limite da gestire:** parità di FLOPS → tempo minore, poi primo nell'ordine [D]; un solo device; unità scalate (MFLOPS/GFLOPS/TFLOPS) [D].
- **Assunzioni / casi esplicitamente non gestiti:** numeri formattati all'italiana da una funzione propria, non dal locale di sistema [D].

#### Comportamento su fallimento
- Il riepilogo segnala sempre ciò che manca.

#### Come
- [P] Risultati accumulati in un array dinamico durante la lettura dei messaggi; `bench_summary_print(results, devices, tests)` in `bench.c`.
- [D] Colonne a larghezza fissa.

#### Test
- CA-6 sui tre device; CA-15 con la variante errata di `add.cl`.

---

### F7 (BE): Test 1D elemento per elemento: `add`, `sub`, `mul`, `div`
**Riferimenti Parte A:** S2, RB-5, RB-6, RB-8, RB-11, CA-5, CA-10, CA-16, CA-18

#### Input
**Parametri / variabili in entrata** — descrittori:
| Nome | Tipo | Obbligatorio | Vincoli / validazione richiesta | P/D |
|---|---|---|---|---|
| `name` / `kernel_name` | stringa | sì | `add`, `sub`, `mul`, `div` | [P] |
| `source_path` | stringa | sì | `minibanchmark/kernels/<nome>.cl` | [P] |
| `work_dim` / `ref_size[0]` | — | sì | 1 / 16.777.216 (2^24) | [P] |
| `shape` / `timeout_s` | — | no | `ANY` / predefinito | [P] |
| `user` | `bench_elementwise_params` | sì | `{ op, rel_tol, abs_tol, min, max, b_nonzero }` | [P] |

Kernel, firma comune [P]:
```c
__kernel void add(__global const float *A, __global const float *B,
                  __global float *C, const unsigned int n)
{
    size_t i = get_global_id(0);
    if (i < n) C[i] = A[i] + B[i];
}
```
`n` per valore (corregge il bug attuale del `size_t` letto come `unsigned int*`); il controllo `i < n` resta come protezione [D].

Dati: A e B da `bench_fill_random_float` con seed fisso in [−1000, 1000]; per `div` B in ±[0,5, 1000] [P].

**Dati letti dal DB:** N/A. **Config letta:** nessuna.

#### Output atteso
**Caso normale:** buffer A, B `READ_ONLY | COPY_HOST_PTR`, C `WRITE_ONLY`, di `eff[0]` [P]; verifica di C contro `op(A[i], B[i])` calcolato sull'host in float, tolleranza relativa 1e-6 e assoluta 1e-6 (`div` relativa 1e-6, OpenCL ammette 2,5 ULP) [P] confronto, [D] valori; FLOP = `eff[0]` (1 per elemento, `div` inclusa) [P].

**Casi di errore**
| Condizione | Codice / eccezione | Cosa restituisce | P/D |
|---|---|---|---|
| `eff[0]` > `UINT_MAX` | errore di `setup` | "dimensione oltre i limiti di `unsigned int`" | [P] |
| Allocazione fallita | — | memoria insufficiente (F4) | [P] |

**Dati scritti/aggiornati sul DB:** N/A.

#### Regole di business applicate
- RB-5 verifica completa; RB-8 aggiunta in due file; RB-11 dimensione effettiva; RB-6 FLOP per convenzione.

#### Casi limite e assunzioni
- **Casi limite da gestire:** L non potenza di 2 (elementi scartati mostrati, CA-18).
- **Assunzioni / casi esplicitamente non gestiti:** kernel limitati dalla banda (GFLOPS bassi, normale); host a 64 bit con SSE.

#### Comportamento su fallimento
- `teardown` libera sempre A, B, C su host e device.

#### Come
- [P] Famiglia generica in `bench/bench_elementwise.c` (`setup`, `set_args`, `verify`, `flop`, `teardown` comuni) con macro `BENCH_ELEMENTWISE_TEST(var, nome, path, op, …)`.
- [P] Funzioni `op_*` e i quattro descrittori in `tests/tests.c`, accanto al registro: aggiungere un test 1D tocca solo il nuovo `.cl` e `tests.c`.
- [P] `kernels/add.cl` sostituito con la nuova firma; nuovi `sub.cl`, `mul.cl`, `div.cl`.

#### Test
- CA-16 (OK su almeno un device), CA-18 (L = 96 → 16.777.152 elementi, 0,00038%), CA-5 e CA-15 (variante `A-B`), CA-10 (test temporaneo `rsub`, `git diff` solo su `rsub.cl` e `tests.c`).

---

### F8 (BE): Test `max` (riduzione)
**Riferimenti Parte A:** S3, RB-2, RB-5, RB-6, RB-11, CA-11, CA-16

#### Input
**Parametri / variabili in entrata** — descrittore:
| Nome | Tipo | Obbligatorio | Vincoli / validazione richiesta | P/D |
|---|---|---|---|---|
| `name` | stringa | sì | `max` | [P] |
| `kernel_name` | stringa | sì | `max_reduce` (`max` è una funzione predefinita di OpenCL C) | [P] |
| `source_path` | stringa | sì | `minibanchmark/kernels/max.cl` | [P] |
| `work_dim` / `ref_size[0]` | — | sì | 1 / 2^24 | [P] |
| `local_mem_bytes(local)` | funzione | sì | `local[0] * sizeof(float)` | [P] |

Kernel (funziona con qualsiasi L, non solo potenze di 2) [P]:
```c
__kernel void max_reduce(__global const float *A, __global float *partial,
                         __local float *scratch, const unsigned int n)
{
    size_t gid = get_global_id(0);
    uint lid = get_local_id(0), s = get_local_size(0);
    scratch[lid] = (gid < n) ? A[gid] : -INFINITY;
    barrier(CLK_LOCAL_MEM_FENCE);
    while (s > 1) {
        uint h = (s + 1) / 2;
        if (lid < s / 2) scratch[lid] = fmax(scratch[lid], scratch[lid + h]);
        barrier(CLK_LOCAL_MEM_FENCE);
        s = h;
    }
    if (lid == 0) partial[get_group_id(0)] = scratch[0];
}
```
Dati: A in [−1000, 1000] con seed fisso, più un massimo unico pari a 5000 in posizione pseudo-casuale [P].

**Dati letti dal DB:** N/A. **Config letta:** nessuna.

#### Output atteso
**Caso normale:** un solo passaggio sul device (massimo per gruppo), chiusura sull'host fuori dalla misura [P]. Buffer A `READ_ONLY | COPY_HOST_PTR` di `eff[0]`, `partial` `WRITE_ONLY` di `eff[0] / L`, `scratch` locale via `clSetKernelArg(…, L * sizeof(float), NULL)` [P]. Verifica con tolleranza 0: ogni `partial[g]` uguale al massimo del suo gruppo calcolato sull'host, e massimo dei `partial` uguale al massimo globale [P]. FLOP = `eff[0]` [P].

**Casi di errore**
| Condizione | Codice / eccezione | Cosa restituisce | P/D |
|---|---|---|---|
| `eff[0]` > `UINT_MAX` | errore di `setup` | come F7 | [P] |
| Local memory insufficiente | — | scartata da F3 (non accade sui device attuali: 4 KB max) | [P] |

**Dati scritti/aggiornati sul DB:** N/A.

#### Regole di business applicate
- RB-2 local memory dichiarata; RB-5 verifica esatta; RB-6 FLOP per convenzione; RB-11 dimensione effettiva.

#### Casi limite e assunzioni
- **Casi limite da gestire:** L non potenza di 2; L = 1.
- **Assunzioni / casi esplicitamente non gestiti:** nessun NaN nell'input; `-INFINITY` disponibile in OpenCL C 1.2.

#### Comportamento su fallimento
- Come F2/F4.

#### Come
- [P] Descrittore e funzioni in `tests/test_max.c`, registrazione in `tests.c`; non usa la famiglia 1D.

#### Test
- CA-16, CA-11; variante errata con `fmin` → ERRATO con indice del gruppo.

---

### F9 (BE): Test `mmul` e `mmul_tiled` (2D)
**Riferimenti Parte A:** S3, RB-2, RB-5, RB-6, RB-11, CA-11, CA-16, CA-17

#### Input
**Parametri / variabili in entrata** — due descrittori:
| Nome | Tipo | Obbligatorio | Vincoli / validazione richiesta | P/D |
|---|---|---|---|---|
| `name` / `kernel_name` | stringa | sì | `mmul`; `mmul_tiled` | [P] |
| `source_path` | stringa | sì | `kernels/mmul.cl`; `kernels/mmul_tiled.cl` | [P] |
| `work_dim` / `ref_size` | — | sì | 2 / {1024, 1024} (N colonne, M righe di C) | [P] |
| K | costante del test | sì | 1024, fisso | [P] |
| `shape` | enum | sì | `mmul`: `ANY`; `mmul_tiled`: `EQUAL` (TS = lato del work-group) | [P] |
| `local_mem_bytes` | funzione | solo tiled | `2 · TS · TS · 4` B | [P] |

Kernel [P]: `col = get_global_id(0)`, `row = get_global_id(1)` (accessi contigui, diverso da `ex_3`); firma `mmul(__global const float *A, __global const float *B, __global float *C, const uint M, const uint N, const uint K)`, per `mmul_tiled` in più `__local float *As, __local float *Bs`.

Dati: A[i][k] e B[k][j] funzioni della posizione (hash con seed fisso) in [−1, 1], così C[r][c] non dipende da M e N [P].

**Dati letti dal DB:** N/A. **Config letta:** nessuna.

#### Output atteso
**Caso normale:** verifica contro un riferimento C_ref 1024×1024 calcolato in double sull'host **una volta per device** e riusato per tutte le configurazioni (confronto per r < M, c < N) [P] cache, [D] realizzazione (stato statico nel file del test); tolleranza relativa 1e-4, assoluta 1e-3 [D]; FLOP = `2 · M · N · K` [P].

**Casi di errore**
| Condizione | Codice / eccezione | Cosa restituisce | P/D |
|---|---|---|---|
| K non multiplo di TS (tiled) | errore di `setup` | impossibile con K = 1024, controllato comunque | [P] |
| Calcolo di C_ref fallito per memoria | — | test saltato su quel device con motivo | [P] |

**Dati scritti/aggiornati sul DB:** N/A.

#### Regole di business applicate
- RB-2: `ANY` per `mmul`, `EQUAL` + local memory per `mmul_tiled`; RB-5, RB-6, RB-11.

#### Casi limite e assunzioni
- **Casi limite da gestire:** M ≠ N per forme rettangolari (es. 32×8).
- **Assunzioni / casi esplicitamente non gestiti:** sul device CPU software circa 0,5–2 s per esecuzione (sotto i 10 s); sulla GPU nessun TDR con N = 1024.

#### Comportamento su fallimento
- Come F2/F4.

#### Come
- [P] Due descrittori in `tests/test_mmul.c` con dati e verifica condivisi; registrazione in `tests.c`.

#### Test
- CA-16, CA-11; CA-17 (`mmul_tiled` solo forme quadrate, `mmul` anche rettangolari); variante con `row`/`col` scambiati → ERRATO.

---

### F10 (BE): Test `stencil3d` (3D, 7 punti)
**Riferimenti Parte A:** S3, RB-2, RB-5, RB-6, RB-11, CA-11, CA-16, CA-19

#### Input
**Parametri / variabili in entrata** — descrittore:
| Nome | Tipo | Obbligatorio | Vincoli / validazione richiesta | P/D |
|---|---|---|---|---|
| `name` / `kernel_name` | stringa | sì | `stencil3d` | [P] |
| `source_path` | stringa | sì | `minibanchmark/kernels/stencil3d.cl` | [P] |
| `work_dim` / `shape` | — | sì | 3 / `ANY` | [P] |
| `ref_size` | — | sì | {256, 256, 256} | [P] |

Formula: `out = c0·in[z][y][x] + c1·(somma dei 6 vicini)`, c0 = 0,4, c1 = 0,1 [D]. Bordi della griglia effettiva copiati invariati (`out = in`), stencil solo sulle celle interne [P].

Kernel [P]: `x = get_global_id(0)`, `y = get_global_id(1)`, `z = get_global_id(2)`, indice `(z·Y + y)·X + x`; firma `stencil3d(__global const float *in, __global float *out, const uint X, const uint Y, const uint Z)`.

Dati: `in` funzione della posizione (hash con seed fisso) in [−1, 1] [P].

**Dati letti dal DB:** N/A. **Config letta:** nessuna.

#### Output atteso
**Caso normale:** buffer `in` `READ_ONLY | COPY_HOST_PTR`, `out` `WRITE_ONLY`, di X·Y·Z effettivi [P]; riferimento calcolato sull'host in double per ogni configurazione (il bordo dipende dalla dimensione effettiva) [P]; tolleranza relativa 1e-5, assoluta 1e-6 [D]; FLOP = `8 · (X−2)(Y−2)(Z−2)` [P].

**Casi di errore**
| Condizione | Codice / eccezione | Cosa restituisce | P/D |
|---|---|---|---|
| Lato effettivo < 3 | errore di `setup` | impossibile con lato 256, controllato comunque | [P] |
| X·Y·Z > `UINT_MAX` | errore di `setup` | | [P] |

**Dati scritti/aggiornati sul DB:** N/A.

#### Regole di business applicate
- RB-2 forme 3D con lati potenze di 2 entro i limiti per dimensione; RB-5, RB-6, RB-11.

#### Casi limite e assunzioni
- **Casi limite da gestire:** forme molto sbilanciate (256×1×1, 1×1×64) valide e misurate.
- **Assunzioni / casi esplicitamente non gestiti:** almeno 3 dimensioni (garantito da OpenCL, controllato da F3); device CPU software 0,1–0,5 s per esecuzione.

#### Comportamento su fallimento
- Come F2/F4.

#### Come
- [P] Descrittore e funzioni in `tests/test_stencil3d.c`, registrazione in `tests.c`.

#### Test
- CA-19, CA-16, CA-11; variante con un vicino omesso → ERRATO con indice lineare.

---

### F11 (BE): `main`, compilazione e test di prova
**Riferimenti Parte A:** A8, S4, RB-9, CA-3, CA-12, CA-13, CA-21

#### Input
**Parametri / variabili in entrata:** `argc`/`argv`, passati a `bench_main`.

**Dati letti dal DB:** N/A. **Config letta:** macro di compilazione `BENCH_SELFTEST`, `BENCH_SELFTEST_HANG`.

#### Output atteso
**Caso normale:** `main.c` [P]:
```c
#include "bench/bench.h"
#include "tests/tests.h"

int main(int argc, char **argv)
{
    bench_options options = BENCH_OPTIONS_DEFAULT; /* 10 s, 1+5 esecuzioni, 120 s inattività */
    return bench_main(argc, argv, BENCH_TESTS, BENCH_TESTS_COUNT, &options);
}
```

Test di prova in `tests/test_selftest.c` [P]:
| Test | Attivo con | Cosa fa | Verifica |
|---|---|---|---|
| `selftest_timeout` | `-DBENCH_SELFTEST` | kernel con ciclo lunghissimo (risultato scritto in output), `timeout_s = 1` | CA-13 |
| `selftest_localmem` | `-DBENCH_SELFTEST` | kernel banale con `local_mem_bytes = L · 128` B | CA-3 |
| `selftest_crash` | `-DBENCH_SELFTEST` | `setup` che scrive all'indirizzo `NULL` | crash di un figlio (F5) |
| `selftest_hang` | `-DBENCH_SELFTEST_HANG` | `setup` che attende 130 s con un ciclo su `clock()` | CA-21 |

Compilazione dalla radice del repo [P]:
```bash
gcc -std=c11 -O2 -Wall -Wextra \
    -I install/include -I minibanchmark \
    minibanchmark/main.c minibanchmark/bench/*.c minibanchmark/tests/*.c \
    -o minibenchmark.exe -L install/lib -lOpenCL -lm
```
Su Linux lo stesso comando con `-o minibenchmark` e la libreria OpenCL di sistema. Esecuzione: `./minibenchmark.exe` dalla radice. Varianti di prova: aggiungere `-DBENCH_SELFTEST` e/o `-DBENCH_SELFTEST_HANG`.

**Casi di errore:** N/A (gestiti da F1–F6).

**Dati scritti/aggiornati sul DB:** N/A.

#### Regole di business applicate
- RB-9: `main.c` identico in ogni progetto; cambiano solo `tests.h`/`tests.c`.

#### Casi limite e assunzioni
- **Casi limite da gestire:** N/A.
- **Assunzioni / casi esplicitamente non gestiti:** `*.exe` già ignorato da `.gitignore`; il nome dell'eseguibile resta `minibenchmark.exe`.

#### Comportamento su fallimento
- N/A.

#### Come
- [P] Eliminare `minibanchmark/minibanchmark.c`; aggiornare `README.md` con una sezione "minibanchmark" (comandi Windows e Linux, varianti di prova).

#### Test
- CA-12: copiare `bench/` + `main.c` in una cartella vuota con un test di prova e compilare.
- [D] Build senza warning con `-Wall -Wextra` (obiettivo, non blocco).

---

### B4. Rischi e punti di attenzione
- **Buffer di `stdout` nel figlio su Windows:** in una pipe il CRT bufferizza a blocchi anche con `_IOLBF`; serve `fflush` esplicito dopo ogni messaggio, altrimenti il padre vede `RUN_START` in ritardo e il timeout scatta per errore.
- **TDR della GPU (~2 s):** un kernel lungo viene interrotto dal sistema; le dimensioni dei test sono scelte per restare sotto. Non modificare `TdrDelay` nel registro.
- **Durata complessiva:** sul device CPU software `stencil3d` (~130 forme) e `mmul` (~30) possono richiedere diversi minuti; stima complessiva 10–15 minuti. Se troppo, le dimensioni di riferimento si cambiano aggiornando la commissione.
- **OpenCLOn12:** strato di traduzione su D3D12; valori di `PREFERRED_WORK_GROUP_SIZE_MULTIPLE`, risoluzione del profiling e limiti per dimensione da verificare alla prima esecuzione; le tolleranze potrebbero richiedere aggiustamenti [D].
- **Dopo la terminazione di un figlio sulla GPU** il driver può impiegare qualche istante a liberare il device: il figlio successivo potrebbe fallire nella creazione del context (errore per quella coppia, nessun blocco).
- **`CL_KERNEL_WORK_GROUP_SIZE`** può essere minore del massimo del device per kernel con molti registri (`mmul_tiled`): F3 lo usa già come limite.
- **Indici a 32 bit** nei kernel: nessun overflow con le dimensioni scelte, ma i controlli in `setup` vanno mantenuti.
- **Linux non verificato** (Q-18): il ramo `__linux__` di `bench_proc.c` sarà compilato e provato più avanti.

### B5. Note per l'implementazione
- Scelte [D] da annotare implementando: `bench_run` pubblico o opaco; algoritmo della mediana; pulizia con `goto cleanup`; intervallo di attesa su Windows; formato delle righe e unità dei FLOPS; regola di parità; funzione di formattazione all'italiana; coefficienti dello stencil; tolleranze; realizzazione della cache di `mmul`; test unitari facoltativi di `bench_configs_generate` e `bench_compare_float`.
- Da ricordare: `CL_USE_DEPRECATED_OPENCL_1_2_APIS` prima di `#include <CL/cl.h>`; il kernel di riduzione si chiama `max_reduce`; `-INFINITY` come valore neutro; `#ifdef BENCH_SELFTEST` / `#ifdef BENCH_SELFTEST_HANG` in `tests.c`; correggere i problemi del codice attuale elencati in A9.

### B6. Piano di rilascio
- **Ordine dei commit / branch / fasi:** branch `refactor-minibanchmark`; ogni passo compila e gira.
  0. Commit del prototipo attuale così com'è ("wip: prototipo minibanchmark").
  1. Struttura: `bench.h`, `main.c`, `tests.c` vuoto, comando di compilazione; F1.
  2. F2, F3 in 1D, F4 ancora nello stesso processo (senza figli), F7, F6: `add`/`sub`/`mul`/`div` misurati e riepilogati.
  3. F5 su Windows: processi figli, timeout, inattività; test di prova timeout/crash/hang.
  4. F3 in 2D/3D, F8, F9, F10, `selftest_localmem`.
  5. Eliminazione di `minibanchmark.c`, README, verifica completa dei CA su Windows; merge su `main`.
  6. (Più avanti, Q-18) Ramo Linux di `bench_proc.c` e CA-20.
- **Migrazione di dati esistenti:** nessuna.
- **Retrocompatibilità e installazioni già in produzione:** gli altri esercizi (`ex_*`, `src/`) non vengono toccati; cambia il formato dell'output di `minibenchmark.exe`, che nessun altro programma legge.
- **Come si torna indietro se va male:** si abbandona il branch o si fa `git revert`; il prototipo resta nel commit 0.

---

## Domande aperte e decisioni
| ID | Domanda | Fase | Rivolta a | Bloccante? | Stato | Decisione |
|---|---|---|---|---|---|---|
| Q-1 | Quali tipi di kernel deve supportare la struttura? | A | io | sì | chiusa | Anche kernel con geometria e argomenti propri (2D, local memory): taglia L |
| Q-2 | Quanti test devono funzionare a fine commissione? | A | io | sì | chiusa | Tutti: `add`, `sub`, `mul`, `div`, `max`, più `mmul` 2D |
| Q-3 | Cosa significa "device migliore"? | A | io | sì | chiusa | Tempo di sola esecuzione del kernel più basso (per ora); superata da Q-15 |
| Q-4 | `max` è elemento per elemento o riduzione? | A | io | sì | chiusa | Riduzione: massimo dell'intero array |
| Q-5 | Dimensione dei dati fissa o adattata al device? | A | io | sì | chiusa | Fissa, dichiarata dal test; se non entra in memoria il test è saltato |
| Q-6 | Cosa succede al superamento del tempo massimo? | A | io | sì | chiusa | TIMEOUT; salto delle configurazioni rimanenti di quel test su quel device; si prosegue col test successivo |
| Q-7 | Dove si configura il tempo massimo? | A | io | sì | chiusa | Valore globale predefinito 10 s nel codice, sovrascrivibile per singolo test |
| Q-8 | Chi stabilisce le configurazioni di work-group? | A | io | sì | chiusa | Il benchmark, in automatico dalle caratteristiche del device: dalla dimensione minima ai multipli fino al massimo (rev. 2) |
| Q-9 | "Multipli": raddoppiando o tutti? | A | io | sì | chiusa | Tutti i multipli (32, 64, 96, …) |
| Q-10 | In 2D quali forme e come gestire kernel con forme obbligate? | A | io | sì | chiusa | Tutte le forme rettangolari valide; il test può dichiarare un vincolo di forma (es. "solo quadrate"), mai l'elenco |
| Q-11 | Configurazioni che non dividono la dimensione dei dati? | B→A | io | sì | chiusa | Per ogni configurazione si calcola prima la dimensione effettiva (multiplo più grande ≤ riferimento) e si popolano i buffer con quella; output con dati elaborati e % scartata (rev. 3, RB-11) |
| Q-12 | Come fermare davvero un kernel oltre il tempo massimo? | B→A | io | sì | chiusa | Processo figlio per ogni coppia test/device, terminato dal padre al timeout; TDR del sistema = TIMEOUT (rev. 3, RB-10) |
| Q-13 | Sistemi operativi supportati? | B→A | io | sì | chiusa | Windows e Linux |
| Q-14 | Kernel 3D supportati? | A | io | sì | chiusa | Sì, oltre a 1D e 2D (rev. 3, RB-2) |
| Q-15 | Criterio della classifica: tempo grezzo o throughput, visto che la dimensione effettiva cambia per configurazione? | B→A | io | sì | chiusa | FLOPS calcolati (FLOP dichiarati dal test / tempo); il tempo resta visibile (rev. 4, RB-6) |
| Q-16 | Quale test 3D aggiungere per verificare il supporto 3D? | A | io | sì | chiusa | Stencil 3D a 7 punti (`stencil3d`); la somma di volumi è scartata perché appiattibile a un vettore |
| Q-17 | Dove sta il riconoscimento del sistema operativo? | B | io | no | chiusa | Nella parte comune (`bench_proc.c`), non nel `main` |
| Q-18 | Su quale macchina Linux si verifica CA-20? | B | io | no | aperta | Rimandata: verrà fatta su un'altra macchina appena disponibile |
| Q-19 | Come si gestiscono le versioni di OpenCL e C? | B | io | sì | chiusa | `CL_TARGET_OPENCL_VERSION 300`; versione del device letta a runtime (≥ 2.0 → `clCreateCommandQueueWithProperties`, 1.x → `clCreateCommandQueue`); kernel senza `-cl-std`; `-std=c11` con controllo `__STDC_VERSION__` in `bench.h` |
| Q-20 | Dove sta la famiglia di test "elemento per elemento"? | B | io | no | chiusa | In `bench/`, helper opzionale e generico; vincoli di forma solo `ANY`/`EQUAL` |
| Q-21 | Come si verifica CA-9 (nessuna piattaforma)? | B | io | no | chiusa | Dato per appurato: non si verifica in questa commissione |
| Q-22 | Device esclusi (non disponibili o senza compilatore): mostrati o nascosti? | B | io | no | chiusa | Mostrati nell'elenco con la marcatura "escluso" e il motivo |
| Q-23 | Quante forme provare in 2D/3D (tutte arrivano a migliaia)? | B→A | io | sì | chiusa | 1D tutti i multipli; 2D/3D lati potenze di 2 con prodotto multiplo della dimensione minima (rev. 5, RB-2) |
| Q-24 | Configurazioni scartate: una riga ciascuna o raggruppate? | B→A | io | no | chiusa | Raggruppate per motivo con il numero (rev. 5, RB-2) |
| Q-25 | Output per configurazione o solo la migliore? | B | io | no | chiusa | Una riga per ogni configurazione misurata, più il riepilogo |
| Q-26 | Un errore durante l'esecuzione del kernel conta come TIMEOUT? | B | io | no | chiusa | Sì, TIMEOUT "fermato dal sistema operativo" |
| Q-27 | Cosa succede se il figlio si blocca fuori dalle esecuzioni? | B→A | io | sì | chiusa | Controllo di inattività: 120 s senza messaggi → processo terminato, "il processo non risponde" (rev. 6, RB-12) |
| Q-28 | Come si verifica il timeout (CA-13)? | B | io | no | chiusa | Test di prova registrato solo compilando con `-DBENCH_SELFTEST` |
| Q-29 | Sezione per test del riepilogo: solo il migliore o classifica? | B | io | no | chiusa | Classifica di tutti i device, migliore marcato |
| Q-30 | Codice di uscita del programma? | B | io | no | chiusa | 0 se l'esecuzione è completa (anche con ERRATO/TIMEOUT), 1 solo per errori fatali |
| Q-31 | Dimensione di riferimento dei test 1D? | B | io | no | chiusa | 2^24 = 16.777.216 elementi (CA-18 aggiornato, rev. 7) |
| Q-32 | Dove stanno descrittori e `op_*` della famiglia 1D? | B | io | no | chiusa | In `tests/tests.c`, accanto al registro (CA-10: si toccano solo `.cl` e `tests.c`) |
| Q-33 | Riduzione `max`: quanto si misura? | B | io | no | chiusa | Un passaggio sul device (massimo per gruppo), chiusura sull'host fuori misura; input con un massimo unico a 5000 |
| Q-34 | Quale `mmul`? | B→A | io | sì | chiusa | Entrambi: `mmul` (memoria globale, forme libere) e `mmul_tiled` (local memory, `EQUAL`, copre CA-17); otto test (rev. 8) |
| Q-35 | Come si verifica CA-3 (local memory insufficiente)? | B | io | no | chiusa | Test di prova con local memory `L · 128` B, attivo solo con `-DBENCH_SELFTEST` |
| Q-36 | `stencil3d`: bordi e dimensione? | B | io | no | chiusa | Bordi copiati invariati, stencil solo sulle celle interne; 256³ |
| Q-37 | Dove sta il test di prova del blocco (CA-21)? | B | io | no | chiusa | Opzione separata `-DBENCH_SELFTEST_HANG` |
| Q-38 | Architettura dei test (B0)? | B | io | sì | chiusa | Alternativa A: descrittore + funzioni del test, con helper |
| Q-39 | Piano di rilascio? | B | io | no | chiusa | Branch `refactor-minibanchmark`, commit 0 del prototipo attuale, passi 1–6 di B6 |
