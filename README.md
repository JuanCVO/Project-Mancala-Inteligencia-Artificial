# Project-Mancala-Inteligencia-Artificial

Proyecto final que integra **paralelización con instrumentación** (motores de IA
para Kalah(6,4): Alfa-Beta y MCTS con OpenMP) y **despliegue distribuido** sobre
Kubernetes (local y nube), con CI/CD y análisis de rendimiento.

## Integrantes

| Nombre | Código | Rol |
|---|---|---|
| Juan Camilo Vélez Ospina | 2510206 | Motor C++ Alfa-Beta + `board.h` |
| Andrés Felipe Salcedo Buitrago | 2359304 | Motor C++ MCTS + benchmarks |
| Juan David Ascencio | 2359660 | Backend (FastAPI) + Frontend |
| Juan Manuel Pérez Cruz | 2266033 | CI/CD + análisis comparativo + documentación |
| Juan Alejandro Urrego | 2569068 | Despliegue local y nube (Kubernetes) |

## Estructura

```
.
├── motor/        # Motores C++ (Alfa-Beta + MCTS), tests, benchmark y Dockerfile
├── backend/      # API FastAPI (Python) que envuelve el motor + tests pytest
├── frontend/     # Interfaz web estática (HTML/CSS/JS) servida con nginx
├── deploy/       # docker-compose y manifiestos Kubernetes (local y nube)
├── docs/         # Documentación (8 secciones)
└── .github/      # Workflows de CI/CD (motor-ci, backend-ci, docker-publish, sonar)
```

## Build local

### Solo el motor

```bash
cd motor
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
cd build && ctest --output-on-failure
```

Ver [motor/README.md](motor/README.md) para probarlo vía Docker y el contrato del
endpoint `/move`.

### Levantar el stack completo (motor + backend + frontend)

```bash
cd deploy/local
docker compose up --build
```

Frontend en `http://localhost:8080`, backend en `http://localhost:8000`
(motor interno en `8001`).

## CI/CD

Cuatro workflows en `.github/workflows/` se ejecutan sobre `main`/`master`:

- `motor-ci.yml` — compila el motor y corre `ctest` (board + alphabeta + mcts).
- `backend-ci.yml` — instala dependencias y corre `pytest` del backend.
- `docker-publish.yml` — build y push de imágenes con tag inmutable (SHA del commit).
- `sonar.yml` — análisis estático con SonarQube/SonarCloud + quality gate.

Detalle en [docs/06-cicd.md](docs/06-cicd.md).

## Documentación

Índice completo en [docs/](docs/README.md) — 8 secciones (arquitectura, motor,
paralelización, despliegue local/nube, CI/CD, análisis comparativo, conclusiones).
