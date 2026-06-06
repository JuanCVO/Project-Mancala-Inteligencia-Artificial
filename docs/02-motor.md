# 02 — Motor de Juego: Reglas Kalah(6,4) y Algoritmos de Búsqueda

## 1. Reglas de Kalah(6,4)

### 1.1 Configuración del tablero

El tablero tiene **14 posiciones** representadas como un arreglo de enteros:

| Índice | Contenido |
|--------|-----------|
| 0 – 5  | Hoyos del jugador 0 (izquierda a derecha) |
| 6      | Kalaha (almacén) del jugador 0 |
| 7 – 12 | Hoyos del jugador 1 (izquierda a derecha) |
| 13     | Kalaha (almacén) del jugador 1 |

Estado inicial: **4 semillas** en cada hoyo, 0 en ambos almacenes (48 semillas en total).

```
Player 1 →   [12][11][10][ 9][ 8][ 7]
             [ 0][ 0][ 0][ 0][ 0][ 0]  ← stores
Player 0 →   [ 0][ 1][ 2][ 3][ 4][ 5]
```

### 1.2 Mecánica de siembra

En su turno el jugador toma **todas** las semillas de uno de sus hoyos y las distribuye una a una en sentido **antihorario**, siguiendo la secuencia de índices:

- **Jugador 0** siembra: 0→1→2→3→4→5→6(propio)→7→8→9→10→11→12 (salta el 13)
- **Jugador 1** siembra: 7→8→9→10→11→12→13(propio)→0→1→2→3→4→5 (salta el 6)

### 1.3 Reglas especiales

| Regla | Condición | Efecto |
|-------|-----------|--------|
| **Turno extra** | Última semilla cae en el kalaha propio | El mismo jugador vuelve a mover |
| **Captura** | Última semilla cae en un hoyo propio vacío AND el hoyo opuesto del rival tiene semillas | Se capturan ambos grupos hacia el kalaha propio |
| **Fin de juego** | Todos los hoyos de un lado quedan vacíos | Las semillas restantes del otro lado van al almacén de su dueño; gana quien tenga más semillas en su kalaha |

### 1.4 Hoyo opuesto

El hoyo opuesto al índice $i$ es:

$$\text{opposite}(i) = \text{STORE\_1} - 1 - i = 12 - i$$

Esto mapea correctamente: hoyo 0 ↔ hoyo 12, hoyo 1 ↔ hoyo 11, …, hoyo 5 ↔ hoyo 7.

---

## 2. Motor Alfa-Beta (JuanCVO)

### 2.1 Función de evaluación heurística

La función heurística usada para evaluar posiciones no terminales desde la perspectiva del jugador `side`:

$$h(\text{estado}) = (k_{\text{propio}} - k_{\text{rival}}) + \alpha \cdot (s_{\text{propio}} - s_{\text{rival}})$$

Donde:
- $k_{\text{propio}}$, $k_{\text{rival}}$: semillas en los almacenes propio y rival
- $s_{\text{propio}}$, $s_{\text{rival}}$: semillas en los hoyos activos (no almacén) de cada lado
- $\alpha = 0.5$ (ponderación del diferencial de semillas en hoyos)

La posición del almacén tiene peso 1.0 y las semillas en hoyos tienen peso 0.5, reflejando que las semillas en hoyos son potencialmente capturables por el rival.

### 2.2 Pseudocódigo — Minimax puro (referencia de corrección)

```
función minimax(tablero, side, current_side, depth) → (move, score)
    si depth == 0 ∨ is_terminal(tablero):
        devolver (-1, h(tablero, side))

    movimientos ← legal_moves(tablero, current_side)
    si movimientos vacío:
        devolver minimax(tablero, side, 1-current_side, depth-1)

    maximizando ← (current_side == side)
    mejor_score ← -∞ si maximizando, +∞ si no
    mejor_mov   ← movimientos[0]

    para cada mov en movimientos:
        nb        ← copia(tablero)
        next_side ← apply_move(nb, mov, current_side)
        nd        ← depth si turno_extra else depth-1
        (_, score) ← minimax(nb, side, next_side, nd)

        si maximizando ∧ score > mejor_score:
            mejor_score ← score; mejor_mov ← mov
        si ¬maximizando ∧ score < mejor_score:
            mejor_score ← score; mejor_mov ← mov

    devolver (mejor_mov, mejor_score)
```

### 2.3 Pseudocódigo — Minimax con poda Alfa-Beta

```
función alphabeta(tablero, side, current_side, depth, α, β) → (move, score)
    si depth == 0 ∨ is_terminal(tablero):
        devolver (-1, h(tablero, side))

    movimientos ← legal_moves(tablero, current_side)
    maximizando ← (current_side == side)
    mejor_score ← -∞ si maximizando, +∞ si no
    mejor_mov   ← movimientos[0]

    para cada mov en movimientos:
        nb        ← copia(tablero)
        next_side ← apply_move(nb, mov, current_side)
        nd        ← depth si turno_extra else depth-1
        (_, score) ← alphabeta(nb, side, next_side, nd, α, β)

        si maximizando:
            si score > mejor_score: mejor_score ← score; mejor_mov ← mov
            α ← max(α, mejor_score)
        si ¬maximizando:
            si score < mejor_score: mejor_score ← score; mejor_mov ← mov
            β ← min(β, mejor_score)

        si β ≤ α: cortar (poda)   ← β-cut o α-cut

    devolver (mejor_mov, mejor_score)
```

**Invariante de corrección:** `alphabeta(b, side, side, d, -∞, +∞).move == minimax(b, side, side, d).move` para todo tablero y profundidad $d$. Este invariante es verificado automáticamente por `test_alphabeta.cpp`.

### 2.4 Manejo del turno extra

Cuando `apply_move` devuelve el mismo `side` (turno extra), la profundidad **no se decrementa**. Esto evita que los turnos extra inflen artificialmente la profundidad efectiva del árbol y mantiene la comparabilidad entre posiciones con y sin turno extra.

### 2.5 Evidencia de corrección

Para ejecutar la suite de pruebas:

```bash
cd motor
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
cd build && ctest --output-on-failure
```

Salida esperada:

```
Board tests: 9 passed, 0 failed
Alpha-Beta tests: 9 passed, 0 failed
```

Los tests verifican explícitamente:
- `alphabeta_seq` produce el mismo movimiento óptimo que `minimax_seq` a profundidades 1, 2 y 4.
- La versión paralela (`alphabeta_par`) produce el mismo movimiento que la secuencial.
- El conteo de nodos de AB es estrictamente menor que el de Minimax (la poda efectivamente elimina ramas).
- Las semillas se conservan durante una partida completa guiada por AB.

---

## 3. Motor MCTS (Andres Felipe)

Implementación: [motor/src/mcts.cpp](../motor/src/mcts.cpp)  
Interfaz pública: [motor/src/mcts.h](../motor/src/mcts.h)

### 3.1 Pseudocódigo UCT — las 4 fases canónicas

```
función mcts_search(tablero, side, simulations, threads) → MCTSResult
    raíz ← nuevo_nodo(tablero, side, mover=-1)

    para iter = 1 … simulations/threads:

        ── FASE 1: Selección ─────────────────────────────────────────────
        nodo ← raíz
        mientras ¬es_terminal(nodo.tablero) ∧ nodo.no_intentados = ∅:
            nodo ← hijo_con_max_UCT(nodo)

        ── FASE 2: Expansión ─────────────────────────────────────────────
        si nodo.no_intentados ≠ ∅:
            mov ← elegir_al_azar(nodo.no_intentados)
            hoja ← nuevo_hijo(nodo, mov)
        sino:
            hoja ← nodo   // nodo terminal: simular desde aquí

        ── FASE 3: Simulación (rollout, paralelizada con OpenMP) ─────────
        total_wins ← 0
        en paralelo para r = 0 … threads-1:
            total_wins += rollout_aleatorio(hoja.tablero, hoja.side)

        ── FASE 4: Retropropagación ──────────────────────────────────────
        para cada nodo en camino desde hoja hasta raíz:
            nodo.N += threads
            si nodo.mover == side:
                nodo.w += total_wins
            sino:
                nodo.w += (threads − total_wins)   // wins para el mover

    devolver hijo_más_visitado(raíz).move_made

función UCT(nodo_hijo) → valor
    si nodo_hijo.N = 0: devolver +∞   // explorar siempre nodos no visitados
    devolver wₙ/Nₙ + c · √(ln(N_padre) / Nₙ)    con c = √2 ≈ 1.4142
```

#### Convención de `w` (wins) y backpropagation

Cada nodo guarda `mover` = el jugador que hizo el movimiento que llevó a ese nodo. `w` acumula victorias para `mover`. En la retropropagación:

- Si `cur->mover == root_side` → `cur->w += total_wins` (el jugador raíz ganó esas simulaciones).
- Si no → `cur->w += (visits − total_wins)` (el oponente de la raíz quiere que ésta pierda).

Esto garantiza que `w/N` mide la tasa de éxito del jugador que eligió ese nodo, haciendo la fórmula UCT coherente: un padre siempre maximiza `w/N` de sus hijos.

#### Selección del movimiento final

Se retorna `child->move_made` del hijo con mayor `N` (más visitado), no el de mayor `w/N`. Esto es más robusto estadísticamente porque refleja consenso a lo largo de toda la búsqueda.

### 3.2 Criterio de corrección estadística de MCTS

MCTS no garantiza el movimiento óptimo. Su corrección es **estadística**: la tasa de coincidencia con el movimiento elegido por Alfa-Beta a profundidad fija debe crecer con el presupuesto de simulaciones.

**Medición:** para cada posición en `tests/suite.txt`, se ejecuta `mcts_search` y `alphabeta_par` (depth=8) y se comprueba si ambos eligen el mismo movimiento.

```bash
# Medir coincidencia para 3 presupuestos:
for SIMS in 1000 10000 100000; do
  OMP_NUM_THREADS=1 ./build/mancala_bench \
    --algo mcts --simulations $SIMS \
    --compare-depth 8 \
    --positions tests/suite.txt
done
```

| Simulaciones | Tasa de coincidencia (10 posiciones) |
|-------------|---------------------------------------|
| 1 000       | reportada en [03-paralelizacion.md](03-paralelizacion.md) §7 |
| 10 000      | reportada en [03-paralelizacion.md](03-paralelizacion.md) §7 |
| 100 000     | reportada en [03-paralelizacion.md](03-paralelizacion.md) §7 |

Se espera que la tasa converja hacia 90–95 % para 100 000 simulaciones en Kalah(6,4), dado que el espacio de juego no es excesivamente grande.

### 3.3 Corrección implementacional

```bash
cd motor
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
cd build && ctest -R MCTSTests --output-on-failure
```

Los 10 tests de `test_mcts.cpp` verifican:
1. Devuelve movimiento legal en tablero inicial (jugador 0 y jugador 1).
2. Devuelve `-1` en tablero terminal.
3. El número de rollouts iguala exactamente al parámetro `simulations`.
4. Mayor presupuesto produce profundidad de árbol igual o mayor.
5. Movimiento único forzado (un solo hoyo con semillas) es elegido correctamente.
6. Paralelización con 2 y 4 hilos produce movimiento legal y cuenta exacta de rollouts.
7. El tablero de entrada no es mutado por `mcts_search`.
8. Posición cerca del final del juego produce movimiento legal.
