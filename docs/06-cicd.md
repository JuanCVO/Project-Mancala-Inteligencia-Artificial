# 06 — CI/CD

## 1. Visión general

El proyecto usa **GitHub Actions** como plataforma de integración y entrega continua. El pipeline cubre cuatro responsabilidades:

1. **Integración continua del motor C++** (`motor-ci.yml`) — compila y ejecuta las pruebas unitarias del motor en cada push y pull request.
2. **Integración continua del backend Python** (`backend-ci.yml`) — instala dependencias y ejecuta la suite `pytest` del backend.
3. **Entrega continua de imágenes** (`docker-publish.yml`) — construye y publica las imágenes Docker de motor, backend y frontend con tags inmutables.
4. **Análisis estático de calidad** (`sonar.yml`) — ejecuta SonarQube/SonarCloud y aplica un quality gate.

Todos los workflows se disparan únicamente sobre las ramas `main` y `master`, que son las únicas evaluadas.

## 2. Diagrama del pipeline

```mermaid
flowchart TD
    A[Push / Pull Request a main] --> B{Workflows}
    B --> C[motor-ci.yml]
    B --> H[backend-ci.yml]
    B --> D[docker-publish.yml]
    B --> E[sonar.yml]
    C --> C1[Instalar cmake, g++, libomp]
    C1 --> C2[Configurar CMake]
    C2 --> C3[Compilar motor]
    C3 --> C4[ctest: board + alphabeta + mcts]
    H --> H1[Setup Python 3.11]
    H1 --> H2[pip install requirements]
    H2 --> H3[pytest backend/tests]
    D --> D1[Login a GHCR]
    D1 --> D2["Build matrix: motor + backend + frontend"]
    D2 --> D4["Push con tag = SHA del commit"]
    E --> E1[Generar compile_commands.json]
    E1 --> E2[sonarsource/sonarqube-scan-action@v2]
    E2 --> E3{Quality Gate}
    E3 -->|Passed| F[Pipeline OK]
    E3 -->|Failed| G[Pipeline falla]
```

## 3. Workflows

### 3.1 motor-ci.yml — Compilación y pruebas del motor

Compila el motor C++ con CMake en modo Release e instala dependencias (`cmake`, `g++`, `libomp-dev` para OpenMP). Ejecuta toda la suite con `ctest` (registra los tests `BoardTests`, `AlphaBetaTests` y `MCTSTests`). Es la evidencia automatizada de corrección: si cualquier test falla (incluido el invariante Alfa-Beta == Minimax), el pipeline falla.

### 3.2 backend-ci.yml — Pruebas del backend

Configura Python 3.11, instala `requirements.txt` y ejecuta `pytest` sobre `backend/tests/`. Las pruebas usan `unittest.mock`/`AsyncMock` y `TestClient` de FastAPI, por lo que **no requieren el motor en ejecución**: validan el contrato de la API (validación Pydantic de `/move`, códigos 200/422/503, probes y CORS) de forma aislada.

### 3.3 docker-publish.yml — Build y push de imágenes

Construye y publica en **Docker Hub** tres imágenes mediante una `matrix`:

| Imagen publicada | Contexto | Tags |
|------------------|----------|------|
| `alejo0213/mancala-motor` | `./motor` | `1.0.0`, `<sha>` |
| `alejo0213/mancala-backend` | `./backend` | `1.0.0`, `<sha>` |
| `alejo0213/mancala-frontend` | `./frontend` | `1.0.0`, `<sha>` |

**Registro: Docker Hub (`alejo0213/*`).** Se eligió Docker Hub para que las imágenes que publica el CI sean exactamente las que despliega Kubernetes: los manifiestos de `deploy/` ya referencian `alejo0213/mancala-*:1.0.0`. El workflow hace login con los secrets `DOCKERHUB_USERNAME` y `DOCKERHUB_TOKEN` (token de acceso de la cuenta `alejo0213`).

Cada imagen se publica con **dos tags inmutables** (nunca `latest`):
- **`1.0.0`** — el que consumen los manifiestos (despliegue = imagen del CI).
- **`<sha>`** (`${{ github.sha }}`) — trazabilidad commit→imagen→pod.

Los puertos ya están alineados: motor en **8001** con probes `/healthz`, backend en 8000 (`/healthz`+`/readyz`), `MOTOR_URL=http://motor-svc:8001`.

### 3.4 sonar.yml — Análisis estático

Usa la action oficial `sonarsource/sonarqube-scan-action@v2` (declarada en YAML, no plugin del marketplace). Analiza el **backend** (Python) y el **frontend** (JS/CSS/HTML), más los Dockerfiles; las pruebas se declaran en `sonar.tests`.

**Decisión documentada:** el **motor C++ no se analiza con Sonar**, sino que se valida en el workflow `motor-ci` (compilación + `ctest`, incluido el invariante Alfa-Beta ≡ Minimax). El analizador C/C++ de Sonar requiere ejecutar el compilador *dentro del contenedor del scanner* (que no lo incluye), por lo que configurarlo excede el alcance del proyecto. El resultado del **quality gate** se consulta en el panel de SonarCloud (ver §6).

## 4. Secrets y configuración

| Secret | Uso | Origen |
|--------|-----|--------|
| GITHUB_TOKEN | Push a GHCR | Automático en Actions |
| SONAR_TOKEN | Autenticación Sonar | Panel de SonarCloud |
| SONAR_HOST_URL | URL del servidor | https://sonarcloud.io |

## 5. Evidencia de ejecuciones

<!-- COMPLETAR: capturas de Actions con los 4 workflows en verde -->

## 6. Quality gate de Sonar

<!-- COMPLETAR: captura del dashboard de SonarCloud (bugs, vulnerabilidades, code smells, cobertura, duplicación) -->
