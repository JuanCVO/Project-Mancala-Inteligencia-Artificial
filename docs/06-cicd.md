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

Construye y publica en GHCR tres imágenes mediante una `matrix`:

| Imagen | Contexto |
|--------|----------|
| `mancala-motor` | `./motor` |
| `mancala-backend` | `./backend` |
| `mancala-frontend` | `./frontend` |

Los tags son inmutables: se usa el SHA del commit (`${{ github.sha }}`), nunca `latest`.

**⚠️ Decisión pendiente con Alejandro — registro/tag.** Hoy hay un desalineamiento:

| | Registro | Imagen | Tag |
|---|---|---|---|
| Este CI (`docker-publish.yml`) | GHCR | `ghcr.io/JuanCVO/mancala-*` | `<sha>` (inmutable por commit) |
| Manifiestos `deploy/` (Alejandro) | Docker Hub | `alejo0213/mancala-*` | `1.0.0` |

Ambos cumplen "no usar `latest`", pero apuntan a sitios distintos. Hay que acordar **uno solo** para que lo que publica el CI sea exactamente lo que despliega Kubernetes. Dos opciones:

- **A) Todo en GHCR + SHA:** Alejandro cambia los manifiestos a `ghcr.io/JuanCVO/mancala-*:<sha>` (máxima trazabilidad commit→imagen→pod).
- **B) Todo en Docker Hub:** este workflow publica a `alejo0213/mancala-*` (requiere secrets `DOCKERHUB_USERNAME`/`DOCKERHUB_TOKEN` y un esquema de tag inmutable, p. ej. `1.0.0`/`<sha>`).

Los puertos ya están alineados: motor en **8001** con probes `/healthz`, backend en 8000 (`/healthz`+`/readyz`), `MOTOR_URL=http://motor-svc:8001`.

### 3.4 sonar.yml — Análisis estático

Usa la action oficial `sonarsource/sonarqube-scan-action@v2` (declarada en YAML, no plugin del marketplace). El alcance (`sonar.sources`) abarca `motor` (C++), `backend` (Python) y `frontend` (JS/CSS); las carpetas de test se declaran en `sonar.tests`. Para el análisis C++ se genera `compile_commands.json` con `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`. El quality gate se aplica con `sonar.qualitygate.wait=true`.

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
