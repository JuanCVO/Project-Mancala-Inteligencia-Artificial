# 04 — Despliegue Local

## Dockerfiles

Cada componente vive en su propio contenedor con su propio Dockerfile:

- `motor/Dockerfile` — compila el binario C++ con OpenMP
- `backend/Dockerfile` — instala dependencias Python con FastAPI
- `frontend/Dockerfile` — sirve los archivos estáticos con nginx

## Docker Compose

El archivo `deploy/local/docker-compose.yml` levanta los tres servicios
con un solo comando desde la raíz del repositorio:

```bash
docker compose -f deploy/local/docker-compose.yml up --build
```

El frontend queda disponible en `http://localhost:8080` y el backend
en `http://localhost:8000`. La variable `OMP_NUM_THREADS=4` se pasa
al motor y al backend vía la sección `environment` del compose.

## Flujo de contenedores

```mermaid
flowchart LR
    user[Usuario\nnavegador] -->|HTTP :8080| front[Frontend\nnginx :80]
    user -->|fetch JSON :8000| back[Backend\nFastAPI :8000]
    back -->|HTTP interno :9090| motor[Motor\nC++/OpenMP :9090]
```

## Variables de entorno

| Variable | Valor | Descripción |
|---|---|---|
| OMP_NUM_THREADS | 4 | Hilos OpenMP para el motor |
| MOTOR_URL | http://motor:9090 | URL interna del motor |

## Kubernetes local con kind

### Requisitos

- kind v0.31+
- kubectl v1.34+

### Crear el clúster

```bash
kind create cluster --name mancala
kubectl cluster-info --context kind-mancala
```

### Aplicar manifiestos

```bash
kubectl apply -f deploy/local/k8s/configmap.yml
kubectl apply -f deploy/local/k8s/motor-deployment.yml
kubectl apply -f deploy/local/k8s/backend-deployment.yml
kubectl apply -f deploy/local/k8s/frontend-deployment.yml
kubectl apply -f deploy/local/k8s/services.yml
```

### Verificar pods

```bash
kubectl get pods,svc,deploy
```

Resultado esperado: 5 pods en estado `Running` —
1 motor + 3 backend + 1 frontend.

![Pods corriendo en kind](img/kubectl-pods-local.png)

### Acceder al frontend

```bash
kubectl port-forward svc/front-svc 8080:80
```

Abrir `http://localhost:8080` en el navegador.

## Diagrama de servicios en Kubernetes

```mermaid
flowchart TB
    user[Usuario] -->|NodePort :30080| front_svc[front-svc\nNodePort]
    user -->|NodePort :30800| api_svc[api-svc\nNodePort]
    front_svc --> front_pod[Frontend Pod\nnginx]
    api_svc --> back1[Backend Pod 1]
    api_svc --> back2[Backend Pod 2]
    api_svc --> back3[Backend Pod 3]
    back1 & back2 & back3 -->|ClusterIP :9090| motor_svc[motor-svc\nClusterIP]
    motor_svc --> motor_pod[Motor Pod\nC++/OpenMP]
```

## ConfigMap del motor

El ConfigMap `motor-config` centraliza las variables compartidas:

| Variable | Valor |
|---|---|
| OMP_NUM_THREADS | 4 |
| DEFAULT_DEPTH | 8 |
| MOTOR_URL | http://motor-svc:9090 |

## Probes de salud

| Contenedor | Liveness | Readiness |
|---|---|---|
| motor | GET /healthz :9090 | GET /healthz :9090 |
| backend | GET /healthz :8000 | GET /readyz :8000 |
| frontend | GET / :80 | — |