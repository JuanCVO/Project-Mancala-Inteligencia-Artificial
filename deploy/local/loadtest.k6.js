// Prueba de carga para el experimento del doc 07 (local vs nube).
// Envia POST /move con Alfa-Beta a profundidad fija y un numero de hilos.
//
// IMPORTANTE: el motor usa el campo "threads" del body (default 1) y ejecuta
// omp_set_num_threads(threads); por eso el barrido se controla con THREADS aqui,
// no con la variable de entorno OMP_NUM_THREADS.
//
// Parametros (variables de entorno -e):
//   TARGET   url del endpoint /move (default backend en localhost:8000)
//   THREADS  hilos OpenMP por peticion: 1, 2, 4 u 8 (default 1)
//   DEPTH    profundidad fija de Alfa-Beta (default 12; usa >=10 para que se note)
//   VUS      usuarios virtuales concurrentes (default 10)
//   DURATION duracion de la corrida sostenida (default 30s)
//
// Salida: k6 imprime http_req_duration (med = p50, p95) y http_reqs (req/s).

import http from 'k6/http';
import { check } from 'k6';

const TARGET   = __ENV.TARGET   || 'http://localhost:8000/move';
const THREADS  = parseInt(__ENV.THREADS  || '1');
const DEPTH    = parseInt(__ENV.DEPTH    || '12');

export const options = {
  vus:      parseInt(__ENV.VUS || '10'),
  duration: __ENV.DURATION || '30s',
};

const board = [4, 4, 4, 4, 4, 4, 0, 4, 4, 4, 4, 4, 4, 0];

export default function () {
  const payload = JSON.stringify({
    board:   board,
    side:    0,
    algo:    'alphabeta',
    depth:   DEPTH,
    threads: THREADS,
  });

  const res = http.post(TARGET, payload, {
    headers: { 'Content-Type': 'application/json' },
  });

  check(res, { 'status 200': (r) => r.status === 200 });
}
