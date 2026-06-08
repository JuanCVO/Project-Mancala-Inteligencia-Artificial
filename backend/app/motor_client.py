"""
HTTP client for the Mancala motor service.
"""
import httpx
import json
import logging
from typing import Dict, Any, Optional
from datetime import datetime
from app.config import MOTOR_URL, MOTOR_TIMEOUT
from app.models import MoveResponse

logger = logging.getLogger(__name__)


class MotorClient:
    """Client for communicating with the C++ motor service."""

    def __init__(self, base_url: str = MOTOR_URL, timeout: int = MOTOR_TIMEOUT):
        self.base_url = base_url
        self.timeout = timeout

    async def move(
        self,
        board: list,
        side: int,
        algo: str = "alphabeta",
        depth: Optional[int] = None,
        simulations: Optional[int] = None,
        threads: int = 1,
    ) -> Dict[str, Any]:
        """
        Call the motor's /move endpoint.

        Returns parsed JSON response.
        """
        payload = {
            "board": board,
            "side": side,
            "algo": algo,
            "threads": threads,
        }
        if depth is not None:
            payload["depth"] = depth
        if simulations is not None:
            payload["simulations"] = simulations

        url = f"{self.base_url}/move"
        try:
            async with httpx.AsyncClient(timeout=self.timeout) as client:
                response = await client.post(
                    url,
                    json=payload,
                    headers={"Content-Type": "application/json"}
                )
                response.raise_for_status()
                return response.json()
        except httpx.TimeoutException:
            logger.error(f"Motor timeout: {url}")
            raise RuntimeError("Motor service timeout")
        except httpx.HTTPError as e:
            logger.error(f"Motor HTTP error: {e}")
            raise RuntimeError(f"Motor service error: {str(e)}")
        except json.JSONDecodeError as e:
            logger.error(f"Invalid JSON from motor: {e}")
            raise RuntimeError("Invalid response from motor service")

    async def healthz(self) -> bool:
        """Check if motor is alive."""
        url = f"{self.base_url}/healthz"
        try:
            async with httpx.AsyncClient(timeout=5) as client:
                response = await client.get(url)
                return response.status_code == 200
        except Exception as e:
            logger.error(f"Healthz check failed: {e}")
            return False

    async def readyz(self) -> bool:
        """Check if motor is ready."""
        url = f"{self.base_url}/readyz"
        try:
            async with httpx.AsyncClient(timeout=5) as client:
                response = await client.get(url)
                return response.status_code == 200
        except Exception as e:
            logger.error(f"Readyz check failed: {e}")
            return False

    async def metrics(self) -> str:
        """Fetch metrics from motor."""
        url = f"{self.base_url}/metrics"
        try:
            async with httpx.AsyncClient(timeout=5) as client:
                response = await client.get(url)
                response.raise_for_status()
                return response.text
        except Exception as e:
            logger.error(f"Metrics fetch failed: {e}")
            return ""


# Global client instance
motor_client = MotorClient()

