# 03 — Paralelización con OpenMP e Instrumentación

## 1. Estrategia para Alfa-Beta: Root Parallelism (JuanCVO)

### 1.1 Estrategia elegida

Se implementó **paralelismo a la raíz** (*root parallelism*): los movimientos legales del nodo raíz se reparten entre hilos mediante `#pragma omp parallel for`. Cada hilo ejecuta una búsqueda Alfa-Beta **secuencial completa** sobre el sub-árbol correspondiente a su movimiento asignado.

```cpp
#pragma omp parallel for schedule(dynamic, 1) default(none) \
    shared(b, moves, scores, thread_nodes, thread_prunes, ...)
for (int i = 0; i < n_moves; ++i) {
    Board nb      = b;
    int next_side = apply_move(nb, moves[i], side);
    ABResult r    = alphabeta_seq(nb, side, next_side, depth - 1,
                                  -INF, INF, ln, lp);
    scores[i] = r.score;
}
// Reducción: tomar el máximo
```

Implementación completa: [motor/src/alphabeta.cpp](../motor/src/alphabeta.cpp) — función `alphabeta_par`.

### 1.2 Costo de sincronización y pérdida de podas

**Costo de sincronización:** mínimo. No existe estado compartido durante la búsqueda — cada hilo trabaja sobre su propia copia del tablero (`Board nb = b`). La única sincronización ocurre al final, en la fase de reducción, que es $O(|\text{moves}|) \leq O(6)$.

**Pérdida de podas:** esta es la principal limitación del esquema. En la búsqueda secuencial, el resultado de explorar el primer movimiento (que típicamente es bueno) estrecha la ventana $[\alpha, \beta]$ para los siguientes. Con root parallelism, cada hilo inicia con la ventana $[-\infty, +\infty]$, perdiendo las podas que habrían derivado del trabajo de los otros hilos.

En la práctica, para Kalah con `depth ≥ 8`, el árbol es suficientemente grande para que el trabajo por hilo sea sustancial y el speedup sea cercano a lineal, compensando la pérdida de podas.

| Aspecto | Root Parallelism | YBWC |
|---------|-----------------|------|
| Complejidad de implementación | Baja | Media |
| Pérdida de podas | Alta | Baja |
| Overhead de sincronización | Mínimo | Moderado |
| Speedup esperado (p=8) | 4×–6× | 5×–7× |

### 1.3 Correctitud

La versión paralela produce el **mismo movimiento óptimo** que la versión secuencial porque:
1. Cada sub-árbol de la raíz es explorado exhaustivamente (sin ventana compartida).
2. La reducción final toma el máximo de los scores, equivalente a la capa maximizadora de la raíz en Minimax.

Verificado por `test_alphabeta.cpp::test_par_vs_seq` y `test_par_vs_seq_4threads`.

---

## 2. Estrategia para MCTS: Leaf Parallelization (Andres Felipe)

### 2.1 Estrategia elegida

Se implementó **paralelismo en la hoja** (*leaf parallelization*): el recorrido del árbol (selección + expansión) es completamente **monocromo** (un solo hilo), y una vez que se alcanza un nodo hoja, se lanzan `p` rollouts independientes en paralelo con `#pragma omp parallel for reduction(+:wins)`, promediando el resultado antes de retropropagarlo como una actualización por lote.

```cpp
// Fragmento de motor/src/mcts.cpp — leaf parallelization
Board lb = leaf->board;
int   ls = leaf->side;
double wins = 0.0;

#pragma omp parallel for reduction(+:wins) schedule(static) num_threads(batch)
for (int r = 0; r < batch; ++r) {
    std::mt19937 local_rng(seed_base + (unsigned)r * 31337u);
    wins += rollout_once(lb, ls, side, local_rng);
}
backprop(leaf, wins, batch, side);   // N += batch, w += wins
```

Implementación completa: [motor/src/mcts.cpp](../motor/src/mcts.cpp).

### 2.2 Justificación frente a alternativas

| Aspecto | Leaf Parallelization | Root Parallelization | Tree Parallelization |
|---------|---------------------|---------------------|---------------------|
| Complejidad de impl. | **Baja** — sin locks | Media — combinar árboles | Alta — locks/atomic + virtual loss |
| Estructura del árbol | Único, compartido | Uno por hilo, fusión al final | Único, compartido con locks |
| Overhead de sync | Mínimo (solo reducción) | Combinación final O(nodos_raíz) | Contención en `atomic` N y w |
| Explora más ramas | No (misma hoja × p) | Sí (p árboles distintos) | Sí (un árbol, p hilos) |
| Reduce varianza por hoja | Sí (promedio de p) | No directamente | No directamente |
| Distribución estadística | Idéntica a secuencial | Puede divergir (sesgo de duplicación) | Puede divergir (sesgo de virtual loss) |

**Decisión:** Leaf parallelization es la opción con mejor relación implementación/corrección. Los rollouts son la parte más costosa computacionalmente (el rollout completo recorre en promedio ~20–40 plies), de modo que paralelizarlos consume la mayor parte del tiempo de búsqueda. El recorrido del árbol (selección + expansión) es $O(\text{profundidad})$, típicamente < 1 % del tiempo total.

### 2.3 Costo de sincronización y propiedades estadísticas

**Costo de sincronización:** mínimo. La única sincronización es la reducción final `reduction(+:wins)` después de `batch` rollouts, equivalente a sumar `p` doubles — $O(p)$ con constante muy pequeña. No existe contención durante los rollouts porque cada hilo trabaja sobre una **copia local** del tablero.

**Propiedad estadística:** los `p` rollouts desde la misma hoja son muestras i.i.d. de la misma distribución estocástica. La retropropagación con `N += p` y `w += Σwins` es equivalente a ejecutar `p` iteraciones secuenciales con la misma hoja, dando la **misma distribución estadística** que MCTS secuencial con el mismo presupuesto total de rollouts.

**Limitación:** leaf parallelization no explora más nodos del árbol — `p` hilos siguen siendo una única selección+expansión por ciclo. Root parallelization o tree parallelization permitirían explorar ramas diferentes simultáneamente, a costa de mayor complejidad y posibles sesgos estadísticos.

---

## 3. Instrumentación

### 3.1 Métricas comunes

Para ambos algoritmos se mide con `std::chrono::steady_clock`:

$$T(p) = \text{tiempo de pared con } p \text{ hilos (segundos)}$$

$$S(p) = \frac{T(1)}{T(p)} \quad \text{(speedup)}$$

$$E(p) = \frac{S(p)}{p} \quad \text{(eficiencia)}$$

### 3.2 Métricas específicas de Alfa-Beta

Por cada par (profundidad, hilos): nodos explorados y podas efectuadas. Expuestas en el JSON de respuesta del motor (`stats.nodes`, `stats.prunes`) y en la salida del benchmark.

### 3.3 Comandos de benchmark

```bash
# Compilar
cd motor && cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --parallel

# ── Alfa-Beta ─────────────────────────────────────────────────────────────
OMP_NUM_THREADS=1 ./build/mancala_bench --algo alphabeta --depth 8  --positions tests/suite.txt
OMP_NUM_THREADS=1 ./build/mancala_bench --algo alphabeta --depth 12 --positions tests/suite.txt
for T in 2 4 8; do
  OMP_NUM_THREADS=$T ./build/mancala_bench --algo alphabeta --depth 8  --positions tests/suite.txt
  OMP_NUM_THREADS=$T ./build/mancala_bench --algo alphabeta --depth 12 --positions tests/suite.txt
done

# ── MCTS ──────────────────────────────────────────────────────────────────
# Barrido completo: 2 presupuestos × 4 conteos de hilos
for SIMS in 10000 100000; do
  for T in 1 2 4 8; do
    OMP_NUM_THREADS=$T ./build/mancala_bench --algo mcts --simulations $SIMS \
      --positions tests/suite.txt
  done
done

# Coincidencia MCTS vs AB a 3 presupuestos
for SIMS in 1000 10000 100000; do
  OMP_NUM_THREADS=1 ./build/mancala_bench --algo mcts --simulations $SIMS \
    --compare-depth 8 --positions tests/suite.txt
done

# ── Profiling (Linux) ─────────────────────────────────────────────────────
# perf stat — contadores de hardware para MCTS con 8 hilos
OMP_NUM_THREADS=8 perf stat -e cycles,instructions,cache-misses,branch-misses \
  ./build/mancala_bench --algo mcts --simulations 100000 --positions tests/suite.txt

# perf stat — Alfa-Beta con 8 hilos
OMP_NUM_THREADS=8 perf stat -e cycles,instructions,cache-misses \
  ./build/mancala_bench --algo alphabeta --depth 12 --positions tests/suite.txt

# /usr/bin/time -v — tiempo de pared + memoria máxima residente
OMP_NUM_THREADS=8 /usr/bin/time -v \
  ./build/mancala_bench --algo mcts --simulations 100000 --positions tests/suite.txt
OMP_NUM_THREADS=8 /usr/bin/time -v \
  ./build/mancala_bench --algo alphabeta --depth 12 --positions tests/suite.txt

# htop — ocupación de núcleos (ejecutar en terminal separado mientras corre el bench)
#   htop -d 5
```

---

## 4. Tablas de T(p), S(p), E(p) — Alfa-Beta

> Las siguientes tablas se completan ejecutando el benchmark real. Los valores mostrados son **placeholders** que deben reemplazarse con los resultados experimentales.

### depth = 8

| Hilos $p$ | $T(p)$ (s) | $S(p)$ | $E(p)$ | Nodos (avg) | Podas (avg) |
|-----------|-----------|--------|--------|-------------|-------------|
| 1         | —         | 1.00   | 1.00   | —           | —           |
| 2         | —         | —      | —      | —           | —           |
| 4         | —         | —      | —      | —           | —           |
| 8         | —         | —      | —      | —           | —           |

### depth = 12

| Hilos $p$ | $T(p)$ (s) | $S(p)$ | $E(p)$ | Nodos (avg) | Podas (avg) |
|-----------|-----------|--------|--------|-------------|-------------|
| 1         | —         | 1.00   | 1.00   | —           | —           |
| 2         | —         | —      | —      | —           | —           |
| 4         | —         | —      | —      | —           | —           |
| 8         | —         | —      | —      | —           | —           |

---

## 5. Tablas de T(p), S(p), E(p) — MCTS

> Ejecutar el barrido del §3.3 y reemplazar los `—` con los valores medidos.
> Los valores de referencia (p=1) deben obtenerse primero; S(p) y E(p) se calculan respecto a ellos.

### simulations = 10 000

| Hilos $p$ | $T(p)$ (s) | $S(p)$ | $E(p)$ | Rollouts totales | Prof. árbol avg |
|-----------|-----------|--------|--------|-----------------|-----------------|
| 1         | —         | 1.00   | 1.00   | 10 000 × 10 pos | —               |
| 2         | —         | —      | —      | 10 000 × 10 pos | —               |
| 4         | —         | —      | —      | 10 000 × 10 pos | —               |
| 8         | —         | —      | —      | 10 000 × 10 pos | —               |

### simulations = 100 000

| Hilos $p$ | $T(p)$ (s) | $S(p)$ | $E(p)$ | Rollouts totales | Prof. árbol avg |
|-----------|-----------|--------|--------|-----------------|-----------------|
| 1         | —         | 1.00   | 1.00   | 100 000 × 10 pos| —               |
| 2         | —         | —      | —      | 100 000 × 10 pos| —               |
| 4         | —         | —      | —      | 100 000 × 10 pos| —               |
| 8         | —         | —      | —      | 100 000 × 10 pos| —               |

**Nota sobre el speedup esperado con leaf parallelization:**
Dado que cada rollout es completamente independiente, el paralelismo es *embarrassingly parallel* para la fase de simulación. El overhead secuencial (selección + expansión del árbol) es pequeño pero no cero, por lo que la ley de Amdahl predice:

$$S(p) \approx \frac{1}{f_s + (1-f_s)/p}$$

donde $f_s$ es la fracción secuencial. Para rollouts de ~30 plies y un árbol de profundidad 5–15, $f_s \approx 5\text{–}15\%$, por lo que se esperan speedups de $S(2) \approx 1.8$, $S(4) \approx 3.0$, $S(8) \approx 4.5$.

---

## 6. Gráficas de Speedup

> Reemplazar los valores de los arreglos `line` con los datos de las tablas §4 y §5 una vez obtenidos.

```mermaid
xychart-beta
    title "Speedup S(p) — Alfa-Beta depth=12"
    x-axis [1, 2, 4, 8]
    y-axis "S(p)" 0 --> 8
    line [1, "—", "—", "—"]
    line [1, 2.0, 4.0, 8.0]
```
*(Azul = AB depth=12 medido; naranja = referencia lineal ideal)*

```mermaid
xychart-beta
    title "Speedup S(p) — MCTS simulations=100000"
    x-axis [1, 2, 4, 8]
    y-axis "S(p)" 0 --> 8
    line [1, "—", "—", "—"]
    line [1, 2.0, 4.0, 8.0]
```
*(Azul = MCTS 100k medido; naranja = referencia lineal ideal)*

**Comandos para generar las gráficas en Python** (una vez obtenidos los datos):

```python
import matplotlib.pyplot as plt

p = [1, 2, 4, 8]
ideal = p  # S(p) = p para referencia

# Reemplazar con valores medidos:
ab_speedup   = [1, None, None, None]   # AB depth=12
mcts_speedup = [1, None, None, None]   # MCTS sims=100000

plt.figure(figsize=(8, 5))
plt.plot(p, ideal,        'k--', label='Ideal lineal')
plt.plot(p, ab_speedup,   'b-o', label='Alpha-Beta depth=12')
plt.plot(p, mcts_speedup, 'r-s', label='MCTS 100k sims')
plt.xlabel('Hilos p'); plt.ylabel('S(p) = T(1)/T(p)')
plt.title('Speedup con OpenMP — Mancala Motor')
plt.legend(); plt.grid(True)
plt.savefig('docs/assets/speedup.png', dpi=150)
```

---

## 7. Coincidencia MCTS vs. Alfa-Beta

Medida con `--compare-depth 8` sobre las 10 posiciones de `tests/suite.txt`.  
Ejecutar el barrido del §3.3 y registrar el campo `coincidence_pct` del JSON de resumen.

| Simulaciones | OMP_NUM_THREADS | Coincidencia con AB depth=8 |
|-------------|-----------------|------------------------------|
| 1 000       | 1               | — %                          |
| 10 000      | 1               | — %                          |
| 100 000     | 1               | — %                          |

**Interpretación:** se espera que la tasa aumente monotónicamente con el presupuesto de simulaciones. Para presupuestos muy pequeños (1 000), el árbol MCTS apenas alcanza profundidad 2–3 y la elección es semi-aleatoria (~50–65 %). Para 100 000, el árbol profundiza lo suficiente para reflejar la evaluación de Alfa-Beta en la mayoría de posiciones de Kalah(6,4) (~88–93 %).

---

## 8. Comparación directa a presupuesto de cómputo equivalente

> Fijando el mismo tiempo de pared por jugada (500 ms con 4 hilos), ¿qué algoritmo elige mejor movimiento con más frecuencia?

**Metodología:** con `T_budget = 500 ms`:
- AB con 4 hilos: usar la mayor profundidad que quepa en 500 ms (determinar ejecutando AB con depth creciente y midiendo).
- MCTS con 4 hilos: usar `simulations` tal que `T(4) ≤ 500 ms` (leer de tabla §5).

| Algoritmo   | Tiempo máx. | Hilos | Profundidad / Simulaciones | % posiciones con "mejor movimiento" según AB-ref |
|-------------|-------------|-------|---------------------------|--------------------------------------------------|
| Alfa-Beta   | 500 ms      | 4     | depth = —                 | — %  (referencia: siempre óptimo a profundidad dada) |
| MCTS        | 500 ms      | 4     | sims = —                  | — %  (coincidencia con AB-ref depth=8)           |

**Discusión esperada:** Alfa-Beta es determinista y garantiza optimalidad a profundidad fija; MCTS es estocástico pero escala mejor en árboles con muchos hijos porque no necesita explorar el árbol completo a cada profundidad. Para Kalah(6,4) con ramificación ≤ 6, AB tiene ventaja en profundidad; para variantes con mayor ramificación, MCTS ganaría.

---

## 9. Herramientas de profiling

Se documentan con capturas en `docs/assets/` (generadas dentro del contenedor Linux):

### 9.1 `perf stat` — contadores de hardware

```
# Salida esperada (reemplazar con captura real):
Performance counter stats for './mancala_bench --algo mcts ...':
    X,XXX,XXX,XXX      cycles
    X,XXX,XXX,XXX      instructions          # X.XX  insn per cycle
           XX,XXX      cache-misses
           XX,XXX      branch-misses
```

*Interpretación:* alta tasa instrucciones/ciclo indica buena utilización de la CPU. Muchos cache-misses en MCTS apuntan a la indirección de punteros del árbol (`std::unique_ptr` + `std::vector`); leaf parallelization reduce esto porque los rollouts son datos locales (tablero copiado en la pila).

### 9.2 `/usr/bin/time -v` — tiempo y memoria

```
# Salida esperada:
    Wall clock (s):           X.XX
    Maximum resident set (KB): X,XXX
    Percent of CPU this job got: XXX%
```

*Interpretación:* con 8 hilos, `Percent of CPU` debería acercarse a 800 %. La memoria máxima residente crece con `simulations` y la profundidad del árbol MCTS (cada nodo almacena un `Board` de 56 bytes + overhead de punteros).

### 9.3 `htop` — ocupación efectiva de núcleos

Ejecutar en terminal paralelo mientras corre el benchmark:
```bash
htop -d 5   # actualizar cada 0.5 s
```
*Evidencia esperada:* con `OMP_NUM_THREADS=8`, las 8 barras de CPU deben mostrar ~100 % durante la fase de rollout paralelo, y caer a ~12.5 % (un hilo) durante la fase de selección/expansión secuencial. Esta alternancia es visible porque cada selección+expansión tarda decenas de µs vs. rollouts de ~1 ms.

> **Insertar capturas de pantalla** de `perf stat`, `/usr/bin/time -v` y `htop` en `docs/assets/` y referenciarlas aquí con `![perf stat](assets/perf_stat_mcts.png)` etc.
