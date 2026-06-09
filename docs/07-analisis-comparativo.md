# 07 — Análisis Comparativo: Local vs. Nube

## 1. Objetivo

Comparar el sistema bajo carga en dos entornos: **local** (una instancia, escalado vertical = más hilos OpenMP) y **nube** (Kubernetes, escalado horizontal = más réplicas). Motor evaluado: Alfa-Beta a profundidad fija en ambos.

## 2. Metodología

Herramienta de carga: <!-- COMPLETAR: wrk / k6 / ab --> sobre `POST /move`.

- Local: una instancia (docker-compose), `OMP_NUM_THREADS ∈ {1,2,4,8}`, profundidad fija = <!-- COMPLETAR -->.
- Nube: `r ∈ {1,3}` réplicas, `OMP_NUM_THREADS=2` fijo, misma profundidad.
- Métricas: latencia p50, latencia p95, throughput sostenido (req/s).

> **⚠️ Notas de montaje (verificadas contra el código real):**
>
> 1. **Punto de carga.** Hay dos entradas posibles:
>    - **Motor directo** (`POST http://<motor>:8001/move`): mide el escalado paralelo del motor sin ruido de la capa web. Recomendado para la tabla **vertical** (hilos {1,2,4,8}).
>    - **Backend** (`POST http://<backend>:8000/move`, FastAPI): mide el sistema extremo a extremo (incluye validación Pydantic + salto httpx al motor). Recomendado para la tabla **horizontal** en nube. El backend **reenvía `threads`** al motor.
> 2. **El número de hilos se controla por el cuerpo de la petición, no por el entorno.** Tanto el motor (`motor/src/server.cpp`) como el backend (`MoveRequest.threads`) tienen `threads` con **default 1**, y el motor ejecuta `omp_set_num_threads(threads)`. Si la petición no envía `"threads": N`, el motor corre con **1 hilo** e **ignora `OMP_NUM_THREADS`** del contenedor. → El cuerpo de carga debe ser, p. ej.:
>    ```json
>    {"board":[4,4,4,4,4,4,0,4,4,4,4,4,4,0],"side":0,"depth":<FIJO>,"threads":<N>,"algo":"alphabeta"}
>    ```
>    y `<N>` es lo que se barre en {1,2,4,8} (local) o se fija en 2 (nube).
> 3. **Despliegue (ya operativo en ambos entornos).**
>    - **Local:** `deploy/local/docker-compose.yml` levanta el stack real (motor:8001 + backend:8000 + frontend:8080).
>    - **Nube/K8s:** los manifiestos (`deploy/cloud/`, `deploy/local/k8s/`) usan imágenes reales `alejo0213/mancala-*:1.0.0` en el puerto **8001** con probes `/healthz`. Service público: `api-svc` (LoadBalancer 80→8000, backend). El motor es `motor-svc` (ClusterIP :8001), así que la carga externa entra por el backend.
>    - **Escalado horizontal (r∈{1,3}):** según la rúbrica se escala el **backend** — `kubectl scale deploy/backend --replicas=<r>` — con `OMP_NUM_THREADS=2` fijo en el ConfigMap. Nota: los 3 backends llaman al mismo `motor-svc` (1 réplica de motor), que puede ser el cuello de botella; comentar ese efecto en §5.

## 3. Resultados — Local (escalado vertical)

| OMP_NUM_THREADS | p50 (ms) | p95 (ms) | Throughput (req/s) |
|---|---|---|---|
| 1 | COMPLETAR | COMPLETAR | COMPLETAR |
| 2 | COMPLETAR | COMPLETAR | COMPLETAR |
| 4 | COMPLETAR | COMPLETAR | COMPLETAR |
| 8 | COMPLETAR | COMPLETAR | COMPLETAR |

<!-- COMPLETAR: gráfica throughput vs. hilos -->

## 4. Resultados — Nube (escalado horizontal)

| Réplicas backend | OMP_NUM_THREADS | p50 (ms) | p95 (ms) | Throughput (req/s) |
|---|---|---|---|---|
| 1 | 2 | COMPLETAR | COMPLETAR | COMPLETAR |
| 3 | 2 | COMPLETAR | COMPLETAR | COMPLETAR |

<!-- COMPLETAR: gráfica throughput vs. réplicas -->

## 5. Observación cualitativa: vertical vs. horizontal

- Vertical (local, más hilos): rendimientos decrecientes por sincronización de cotas α/β y contención de memoria; relacionar con E(p) de 03.
- Horizontal (nube, más réplicas): throughput agregado más lineal (peticiones independientes repartidas por el Service), pero la latencia individual no mejora.
- Trade-off: vertical mejora latencia de una petición; horizontal mejora throughput agregado. Indicar cuál conviene según objetivo.

## 6. Comparativa entre algoritmos a presupuesto equivalente

Remitir a la tabla de coincidencia MCTS vs. Alfa-Beta y a las gráficas de speedup de [03-paralelizacion.md](03-paralelizacion.md).
