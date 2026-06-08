from pydantic import BaseModel, Field, validator
from typing import List, Dict, Any, Optional


class MoveRequest(BaseModel):
    """Request payload for POST /move endpoint."""
    board: List[int] = Field(
        ...,
        min_items=14,
        max_items=14,
        description="Board state: 14 integers"
    )
    side: int = Field(..., ge=0, le=1, description="Player side (0 or 1)")
    algo: str = Field(default="alphabeta", description="Algorithm")
    depth: Optional[int] = Field(default=8, ge=1, le=20)
    simulations: Optional[int] = Field(default=10000, ge=100, le=1000000)
    threads: int = Field(default=1, ge=1, le=64)

    @validator('board')
    def validate_board(cls, v):
        if not all(isinstance(x, int) and x >= 0 for x in v):
            raise ValueError("Board must contain non-negative integers")
        return v

    @validator('algo')
    def validate_algo(cls, v):
        if v not in ("alphabeta", "mcts"):
            raise ValueError("Algorithm must be 'alphabeta' or 'mcts'")
        return v


class MoveResponse(BaseModel):
    move: int
    evaluation: float
    elapsed_ms: int
    stats: Dict[str, Any]
    threads_used: int


class HealthResponse(BaseModel):
    status: str


class ReadyResponse(BaseModel):
    status: str


class MetricsResponse(BaseModel):
    timestamp: str
    metrics: Dict[str, Any]

