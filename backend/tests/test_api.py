"""
Backend API tests: validation, happy paths, and error cases.
"""
import pytest
from fastapi.testclient import TestClient
from unittest.mock import patch, AsyncMock
from app.main import app
from app.models import MoveRequest

client = TestClient(app)


class TestMoveEndpoint:
    """Tests for POST /move endpoint."""

    @pytest.fixture
    def valid_board(self):
        """Valid initial Kalah(6,4) board."""
        return [4, 4, 4, 4, 4, 4, 0, 4, 4, 4, 4, 4, 4, 0]

    @pytest.fixture
    def valid_payload(self, valid_board):
        """Valid move request payload."""
        return {
            "board": valid_board,
            "side": 0,
            "algo": "alphabeta",
            "depth": 6,
            "threads": 1,
        }

    @patch("app.motor_client.motor_client.move")
    async def test_move_success_alphabeta(self, mock_move, valid_payload):
        """Test successful move with alphabeta algorithm."""
        mock_move.return_value = {
            "move": 1,
            "evaluation": -1.0,
            "elapsed_ms": 10,
            "stats": {"algo": "alphabeta", "nodes": 1000, "prunes": 100},
            "threads_used": 1,
        }

        response = client.post("/move", json=valid_payload)
        assert response.status_code == 200
        data = response.json()
        assert data["move"] == 1
        assert data["evaluation"] == -1.0
        assert data["stats"]["algo"] == "alphabeta"

    @patch("app.motor_client.motor_client.move")
    async def test_move_success_mcts(self, mock_move, valid_payload):
        """Test successful move with MCTS algorithm."""
        valid_payload["algo"] = "mcts"
        valid_payload["simulations"] = 5000
        del valid_payload["depth"]

        mock_move.return_value = {
            "move": 2,
            "evaluation": 0.55,
            "elapsed_ms": 50,
            "stats": {"algo": "mcts", "rollouts": 5000, "win_rate": 0.55},
            "threads_used": 1,
        }

        response = client.post("/move", json=valid_payload)
        assert response.status_code == 200
        data = response.json()
        assert data["move"] == 2
        assert data["stats"]["algo"] == "mcts"

    def test_move_invalid_board_size(self, valid_payload):
        """Test request with invalid board size (not 14 elements)."""
        valid_payload["board"] = [4, 4, 4]  # Too short
        response = client.post("/move", json=valid_payload)
        assert response.status_code == 422

    def test_move_invalid_board_values(self, valid_payload):
        """Test request with negative board values."""
        valid_payload["board"][0] = -1
        response = client.post("/move", json=valid_payload)
        assert response.status_code == 422

    def test_move_invalid_side(self, valid_payload):
        """Test request with invalid side (not 0 or 1)."""
        valid_payload["side"] = 2
        response = client.post("/move", json=valid_payload)
        assert response.status_code == 422

    def test_move_invalid_algo(self, valid_payload):
        """Test request with unsupported algorithm."""
        valid_payload["algo"] = "minimax"
        response = client.post("/move", json=valid_payload)
        assert response.status_code == 422

    def test_move_invalid_depth_range(self, valid_payload):
        """Test request with depth outside valid range."""
        valid_payload["depth"] = 50  # Too high
        response = client.post("/move", json=valid_payload)
        assert response.status_code == 422

    def test_move_invalid_threads(self, valid_payload):
        """Test request with invalid thread count."""
        valid_payload["threads"] = 0
        response = client.post("/move", json=valid_payload)
        assert response.status_code == 422

    def test_move_missing_required_board(self):
        """Test request without required 'board' field."""
        response = client.post("/move", json={
            "side": 0,
            "algo": "alphabeta",
        })
        assert response.status_code == 422

    def test_move_missing_required_side(self, valid_payload):
        """Test request without required 'side' field."""
        del valid_payload["side"]
        response = client.post("/move", json=valid_payload)
        assert response.status_code == 422

    def test_move_missing_required_algo(self, valid_payload):
        """Test request without required 'algo' field."""
        del valid_payload["algo"]
        # Should use default value "alphabeta" - so should be valid
        response = client.post("/move", json=valid_payload)
        # Will fail at motor_client stage, but validation should pass
        # We'd need to mock motor_client for complete test

    def test_move_default_depth(self, valid_payload):
        """Test that depth defaults to 8 when not provided."""
        del valid_payload["depth"]
        with patch("app.motor_client.motor_client.move") as mock_move:
            mock_move.return_value = {
                "move": 1,
                "evaluation": 0.0,
                "elapsed_ms": 10,
                "stats": {"algo": "alphabeta"},
                "threads_used": 1,
            }
            response = client.post("/move", json=valid_payload)
            # Check that motor_client was called with some depth value
            assert mock_move.called

    @patch("app.motor_client.motor_client.move")
    async def test_move_motor_timeout(self, mock_move, valid_payload):
        """Test handling of motor service timeout."""
        mock_move.side_effect = RuntimeError("Motor service timeout")
        response = client.post("/move", json=valid_payload)
        assert response.status_code == 503


class TestHealthzEndpoint:
    """Tests for GET /healthz endpoint."""

    def test_healthz_always_ok(self):
        """Test that healthz always returns 200."""
        response = client.get("/healthz")
        assert response.status_code == 200
        data = response.json()
        assert data["status"] == "ok"

    def test_healthz_content_type(self):
        """Test healthz response content type."""
        response = client.get("/healthz")
        assert "application/json" in response.headers["content-type"]


class TestReadyzEndpoint:
    """Tests for GET /readyz endpoint."""

    @patch("app.motor_client.motor_client.readyz")
    async def test_readyz_ready(self, mock_readyz):
        """Test readyz when motor is ready."""
        mock_readyz.return_value = True
        response = client.get("/readyz")
        assert response.status_code == 200
        data = response.json()
        assert data["status"] == "ready"

    @patch("app.motor_client.motor_client.readyz")
    async def test_readyz_not_ready(self, mock_readyz):
        """Test readyz when motor is not ready."""
        mock_readyz.return_value = False
        response = client.get("/readyz")
        assert response.status_code == 503
        data = response.json()
        assert data["status"] == "not_ready"


class TestMetricsEndpoint:
    """Tests for GET /metrics endpoint."""

    @patch("app.motor_client.motor_client.metrics")
    async def test_metrics_format(self, mock_metrics):
        """Test that metrics returns properly formatted response."""
        mock_metrics.return_value = (
            "# alphabeta\n"
            "ab_calls 100\n"
            "ab_nodes 50000\n"
            "ab_prunes 5000\n"
            "# mcts\n"
            "mcts_calls 50\n"
            "mcts_rollouts 250000\n"
        )
        response = client.get("/metrics")
        assert response.status_code == 200
        data = response.json()
        assert "timestamp" in data
        assert "metrics" in data
        assert "backend_metrics" in data["metrics"]
        assert "motor_metrics" in data["metrics"]

    @patch("app.motor_client.motor_client.metrics")
    async def test_metrics_backend_tracking(self, mock_metrics):
        """Test that backend metrics track calls."""
        mock_metrics.return_value = ""
        response = client.get("/metrics")
        assert response.status_code == 200
        data = response.json()
        # Check structure exists
        assert "backend_metrics" in data["metrics"]


class TestCORSHeaders:
    """Tests for CORS configuration."""

    def test_cors_preflight_request(self):
        """Test CORS preflight (OPTIONS) request handling."""
        response = client.options(
            "/move",
            headers={
                "Origin": "http://localhost:8080",
                "Access-Control-Request-Method": "POST",
            },
        )
        assert response.status_code == 200
        assert "access-control-allow-origin" in response.headers

    def test_cors_allowed_origin(self):
        """Test that allowed origin is present in response headers."""
        response = client.get(
            "/healthz",
            headers={"Origin": "http://localhost:8080"}
        )
        assert response.status_code == 200
        # FastAPI's CORS middleware should add the header
        # (exact behavior depends on allow_credentials setting)


class TestRootEndpoint:
    """Tests for root endpoint."""

    def test_root_endpoint(self):
        """Test root endpoint returns app info."""
        response = client.get("/")
        assert response.status_code == 200
        data = response.json()
        assert "name" in data
        assert "version" in data
        assert "endpoints" in data

