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

> Esta sección es completada por Andres Felipe. Ver [03-paralelizacion.md](03-paralelizacion.md) para la comparativa entre ambos motores.

### 3.1 Pseudocódigo UCT

```
función mcts(tablero, side, simulations) → move
    raíz ← nuevo_nodo(tablero, side)

    repetir simulations veces:
        nodo ← selección_UCT(raíz)
        nodo ← expansión(nodo)
        resultado ← simulación_aleatoria(nodo)
        retropropagación(nodo, resultado)

    devolver hijo_más_visitado(raíz).move

función UCT(nodo) → valor
    devolver wₙ/Nₙ + c · √(ln(N_padre) / Nₙ)    con c = √2
```

### 3.2 Criterio de corrección de MCTS

A diferencia de Alfa-Beta, MCTS no garantiza el movimiento óptimo. Su corrección es estadística: para presupuestos crecientes de simulaciones, la tasa de coincidencia con el movimiento óptimo de Alfa-Beta debe converger hacia 1.

| Simulaciones | Tasa de coincidencia esperada |
|-------------|-------------------------------|
| 1 000       | ~60 % |
| 10 000      | ~80 % |
| 100 000     | ~92 % |

*(Valores exactos se reportan en [03-paralelizacion.md](03-paralelizacion.md) tras correr el benchmark.)*
