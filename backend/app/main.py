"""
FastAPI backend wrapper for Mancala game engine.
"""
from fastapi import FastAPI, HTTPException, status
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse
import logging
from datetime import datetime
from typing import Dict, Any
import os

from app import config
from app.models import MoveRequest, MoveResponse, HealthResponse, ReadyResponse, MetricsResponse
from app.motor_client import motor_client

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# Create FastAPI app
app = FastAPI(
    title=config.APP_TITLE,
    description=config.APP_DESCRIPTION,
    version=config.APP_VERSION,
)

# Configure CORS
app.add_middleware(
    CORSMiddleware,
    allow_origins=config.CORS_ORIGINS,
    allow_credentials=True,
    allow_methods=config.CORS_METHODS,
    allow_headers=config.CORS_HEADERS,
)

# Global metrics store
metrics_store: Dict[str, Any] = {
    "alphabeta": {"calls": 0, "total_time_ms": 0, "min_time_ms": float('inf'), "max_time_ms": 0, "errors": 0},
    "mcts": {"calls": 0, "total_time_ms": 0, "min_time_ms": float('inf'), "max_time_ms": 0, "errors": 0},
}


@app.post(
    "/move",
    response_model=MoveResponse,
    status_code=status.HTTP_200_OK,
    summary="Calculate best move",
    description="Invoke the game engine to calculate the best move for a given board state",
)
async def post_move(request: MoveRequest) -> MoveResponse:
    """
    POST /move: Calculate best move using specified algorithm.

    Validates input using Pydantic, calls motor service, and returns result.
    """
    try:
        # Set defaults for optional parameters
        depth = request.depth if request.algo == "alphabeta" else None
        simulations = request.simulations if request.algo == "mcts" else None

        # Call motor service
        motor_response = await motor_client.move(
            board=request.board,
            side=request.side,
            algo=request.algo,
            depth=depth,
            simulations=simulations,
            threads=request.threads,
        )

        # Update metrics
        algo = request.algo
        elapsed = motor_response.get("elapsed_ms", 0)
        metrics_store[algo]["calls"] += 1
        metrics_store[algo]["total_time_ms"] += elapsed
        metrics_store[algo]["min_time_ms"] = min(metrics_store[algo]["min_time_ms"], elapsed)
        metrics_store[algo]["max_time_ms"] = max(metrics_store[algo]["max_time_ms"], elapsed)

        # Build response
        response = MoveResponse(
            move=motor_response.get("move", -1),
            evaluation=motor_response.get("evaluation", 0.0),
            elapsed_ms=elapsed,
            stats=motor_response.get("stats", {}),
            threads_used=motor_response.get("threads_used", request.threads),
        )
        return response

    except Exception as e:
        logger.error(f"Error in /move endpoint: {str(e)}", exc_info=True)
        # Update error metrics
        metrics_store[request.algo]["errors"] += 1
        raise HTTPException(
            status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
            detail=f"Motor service error: {str(e)}",
        )


@app.get(
    "/healthz",
    response_model=HealthResponse,
    status_code=status.HTTP_200_OK,
    summary="Liveness probe",
    description="Check if backend is alive (always returns 200)",
)
async def get_healthz() -> HealthResponse:
    """GET /healthz: Liveness probe."""
    return HealthResponse(status="ok")


@app.get(
    "/readyz",
    response_model=ReadyResponse,
    status_code=status.HTTP_200_OK,
    summary="Readiness probe",
    description="Check if backend and motor are ready to serve requests",
)
async def get_readyz() -> JSONResponse:
    """
    GET /readyz: Readiness probe.

    Returns 200 if motor is available, 503 otherwise.
    """
    motor_ready = await motor_client.readyz()

    if not motor_ready:
        return JSONResponse(
            status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
            content={"status": "not_ready"},
        )

    return JSONResponse(
        status_code=status.HTTP_200_OK,
        content={"status": "ready"},
    )


@app.get(
    "/metrics",
    response_model=MetricsResponse,
    status_code=status.HTTP_200_OK,
    summary="Aggregate metrics",
    description="Retrieve aggregated metrics by algorithm",
)
async def get_metrics() -> MetricsResponse:
    """GET /metrics: Return aggregated metrics."""
    # Parse motor metrics
    motor_metrics_text = await motor_client.metrics()
    motor_metrics = {}

    for line in motor_metrics_text.strip().split('\n'):
        if line.startswith('#') or not line.strip():
            continue
        parts = line.split()
        if len(parts) == 2:
            motor_metrics[parts[0]] = int(parts[1])

    # Build comprehensive metrics response
    metrics = {
        "backend_metrics": metrics_store,
        "motor_metrics": motor_metrics,
    }

    return MetricsResponse(
        timestamp=datetime.utcnow().isoformat(),
        metrics=metrics,
    )


@app.get("/", tags=["info"])
async def root():
    """Root endpoint with API info."""
    return {
        "name": config.APP_TITLE,
        "version": config.APP_VERSION,
        "description": config.APP_DESCRIPTION,
        "endpoints": {
            "move": "POST /move",
            "healthz": "GET /healthz",
            "readyz": "GET /readyz",
            "metrics": "GET /metrics",
            "docs": "/docs (Swagger UI)",
        },
    }


if __name__ == "__main__":
    import uvicorn

    port = int(os.getenv("PORT", 8000))
    uvicorn.run(app, host="0.0.0.0", port=port)

