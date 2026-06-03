# Project-Mancala-Inteligencia-Artificial
Este proyecto integra los dos hilos conductores del curso: paralelizacion con instrumentación sobre un problema de computo intensivo y despliegue distribuido de la solucion sobre Kubernetes en la nube.

---

## Probar el motor de juego (Alpha-Beta) con Docker

### Requisitos
- [Docker Desktop](https://www.docker.com/products/docker-desktop/) instalado y corriendo.

### 1. Construir la imagen

```bash
docker build -t mancala-motor ./motor
```

### 2. Correr el servidor

```bash
docker run --rm -p 8001:8001 --name mancala mancala-motor
```

El servidor queda escuchando en `http://localhost:8001`.

### 3. Probar el endpoint

En otra terminal, con Python:

```bash
python -c "
import urllib.request, json
data = json.dumps({'board':[4,4,4,4,4,4,0,4,4,4,4,4,4,0],'side':0,'depth':6}).encode()
req = urllib.request.Request('http://localhost:8001/move', data=data, headers={'Content-Type':'application/json'})
resp = urllib.request.urlopen(req, timeout=30)
print(json.dumps(json.loads(resp.read().decode()), indent=2))
"
```

Respuesta esperada:
```json
{
  "move": 1,
  "evaluation": -1.0,
  "elapsed_ms": 1,
  "stats": {
    "algo": "alphabeta",
    "nodes": 43039,
    "prunes": 8301
  },
  "threads_used": 1
}
```

### 4. Otros endpoints

| Endpoint | Método | Descripción |
|----------|--------|-------------|
| `/move` | POST | Calcula el mejor movimiento |
| `/healthz` | GET | Liveness probe |
| `/readyz` | GET | Readiness probe |
| `/metrics` | GET | Métricas acumuladas |

### 5. Parámetros del endpoint `/move`

| Campo | Tipo | Default | Descripción |
|-------|------|---------|-------------|
| `board` | array[14] | requerido | Estado del tablero (14 enteros) |
| `side` | int | 0 | Jugador a mover (0 o 1) |
| `depth` | int | 8 | Profundidad de búsqueda |
| `threads` | int | 1 | Hilos OpenMP para la búsqueda paralela |
| `algo` | string | `"alphabeta"` | Algoritmo: `"alphabeta"` o `"mcts"` |

### 6. Detener el servidor

```bash
docker stop mancala
```
