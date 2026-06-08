# Backend API — Mancala Motor Wrapper

FastAPI wrapper para el motor de juego Mancala C++.

## Requisitos

- Python 3.11+
- pip o poetry
- Docker (opcional, para ejecutar con motor)

## Instalación Local

### 1. Crear ambiente virtual

```bash
python -m venv venv
source venv/bin/activate  # En Windows: venv\Scripts\activate
```

### 2. Instalar dependencias

```bash
pip install -r requirements.txt
```

## Desarrollo

### Iniciar servidor local

```bash
python -m uvicorn app.main:app --reload --host 0.0.0.0 --port 8000
```

El servidor estará disponible en `http://localhost:8000`.

**Documentación interactiva:** `http://localhost:8000/docs` (Swagger UI)

### Configuración

Las variables de entorno se leen del archivo `.env` o del ambiente:

```bash
export MOTOR_URL=http://localhost:8001
export PORT=8000
```

## Testing

### Ejecutar tests

```bash
pytest tests/ -v
```

### Coverage

```bash
pytest tests/ --cov=app --cov-report=html
```

Se generará un reporte en `htmlcov/index.html`.

## Endpoints

| Método | Ruta | Descripción |
|--------|------|-------------|
| POST | `/move` | Calcular mejor movimiento |
| GET | `/healthz` | Liveness probe |
| GET | `/readyz` | Readiness probe |
| GET | `/metrics` | Métricas agregadas |

Ver `docs/01-arquitectura.md` para especificación completa.

## Docker

### Construir imagen

```bash
docker build -t mancala-backend .
```

### Ejecutar contenedor

```bash
docker run -p 8000:8000 \
  -e MOTOR_URL=http://host.docker.internal:8001 \
  mancala-backend
```

## Estructura

```
app/
├── __init__.py       # Package initialization
├── main.py           # FastAPI app y rutas
├── models.py         # Esquemas Pydantic
├── config.py         # Configuración (CORS, MOTOR_URL)
└── motor_client.py   # Cliente HTTP del motor

tests/
├── __init__.py
└── test_api.py       # Tests de endpoints
```

## Notas de Desarrollo

- Los esquemas Pydantic validan automáticamente las entradas (HTTP 422 en error)
- CORS está configurado explícitamente sin wildcards
- Métricas se agregan en memoria (no persiste entre reinicios)
- Timeouts: 30s para llamadas al motor

## Troubleshooting

**"Connection refused" al motor:**
```bash
# Verificar que motor está corriendo en puerto 8001
curl http://localhost:8001/healthz
```

**Error de CORS:**
- Verificar que el origen del frontend está en `app/config.py` CORS_ORIGINS
- Usar `http://localhost:8080` (no https en local)

**Tests fallan:**
```bash
# Limpiar cache
rm -rf .pytest_cache __pycache__
pytest tests/ -v
```

---

**Documentación completa:** Ver `docs/01-arquitectura.md`

