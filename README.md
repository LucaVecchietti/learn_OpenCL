## COMPILING

```bash
gcc <filepath> -o <executable_name.extension> \
    -I install/include \
    -L install/lib \
    -lOpenCL
```

## MINIBANCHMARK

Benchmark OpenCL su tutte le piattaforme e i device disponibili: per ogni test prova
automaticamente le configurazioni di work-group, verifica i risultati e alla fine indica
device e configurazione migliori (in FLOPS).
Specifica: `docs/commissioni/commissione_Refactor-minibanchmark.md`.

Si compila e si lancia **dalla radice del repository** (i percorsi dei `.cl` sono relativi).

### Windows (MSYS2)

```bash
gcc -std=c11 -O2 -Wall -Wextra \
    -I install/include -I minibanchmark \
    minibanchmark/main.c minibanchmark/bench/*.c minibanchmark/tests/*.c \
    -o minibenchmark.exe -L install/lib -lOpenCL -lm

./minibenchmark.exe
```

### Linux

```bash
gcc -std=c11 -O2 -Wall -Wextra \
    -I minibanchmark \
    minibanchmark/main.c minibanchmark/bench/*.c minibanchmark/tests/*.c \
    -o minibenchmark -lOpenCL -lm

./minibenchmark
```

### Varianti di prova

Aggiungere al comando di compilazione:

- `-DBENCH_SELFTEST`: test di prova del tempo massimo (`selftest_timeout`), della local
  memory insufficiente (`selftest_localmem`) e del crash di un processo figlio (`selftest_crash`).
- `-DBENCH_SELFTEST_HANG`: test di prova del controllo di inattività (`selftest_hang`);
  allunga l'esecuzione di circa 2 minuti per device.

### Struttura

- `minibanchmark/bench/`: parte comune portabile (scoperta dei device, configurazioni,
  misura, processi figli e timeout, riepilogo). Non conosce nessun test.
- `minibanchmark/tests/`: test di minibanchmark; `tests.c` è l'unico punto di registrazione.
- `minibanchmark/kernels/`: sorgenti `.cl`.
- `minibanchmark/main.c`: punto di ingresso, identico in ogni progetto che usa `bench/`.

Per portare il benchmark in un altro progetto si copiano `bench/` e `main.c` e si scrivono
i propri `tests/tests.h` e `tests/tests.c`.
