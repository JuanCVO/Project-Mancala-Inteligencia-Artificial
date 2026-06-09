# 08 — Conclusiones

## 1. Limitaciones

- La paralelización de Alfa-Beta (root/YBWC) pierde podas porque α y β no se comparten perfectamente entre hilos; speedup por debajo del ideal.
- MCTS no garantiza el óptimo; calidad estadística según presupuesto.
- Experimento de carga sobre conjunto limitado de posiciones.
- Escalado horizontal probado con pocas réplicas (r ∈ {1,3}).

## 2. Retos

- Acordar `board.h` compartido antes de implementar los motores.
- Configurar análisis C++ en Sonar (requiere compile_commands.json).
- Coordinar tags inmutables entre CI/CD y manifiestos k8s.
- Medir speedup/eficiencia de forma reproducible (perf/htop).

## 3. Recomendaciones

- Combinar horizontal (réplicas para throughput) + hilos moderados por réplica (latencia).
- Reducir pérdida de podas con YBWC.
- Subir simulaciones de MCTS donde la latencia lo permita.

## 4. Lecciones aprendidas

- Diferencia práctica vertical vs. horizontal.
- Importancia de la instrumentación (nodos, podas, tiempos) para sustentar con datos.
- Valor del CI/CD para detectar regresiones del motor.
- Trabajo distribuido con puntos de coordinación explícitos.
