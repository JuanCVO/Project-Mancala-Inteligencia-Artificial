"""
Configuration settings for the backend API.
"""
import os
from typing import List


# Motor configuration
MOTOR_URL = os.getenv("MOTOR_URL", "http://localhost:8001")
MOTOR_TIMEOUT = 30  # seconds

# CORS configuration
CORS_ORIGINS: List[str] = [
    "http://localhost:8080",
    "http://localhost:8000",
    "http://127.0.0.1:8080",
    "http://127.0.0.1:8000",
]

CORS_METHODS = ["GET", "POST", "OPTIONS"]
CORS_HEADERS = ["Content-Type", "Accept"]

# App settings
APP_TITLE = "Mancala Motor API"
APP_DESCRIPTION = "FastAPI wrapper for the Mancala game engine"
APP_VERSION = "1.0.0"

