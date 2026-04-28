import os

import pytest
import requests


pytestmark = pytest.mark.skipif(
    os.getenv("RUN_LIVE_NAVIGATION_TESTS") != "1",
    reason="set RUN_LIVE_NAVIGATION_TESTS=1 to call the local Flask server and AMap",
)

BASE_URL = os.getenv("GUIDE_CANE_SERVER_URL", "http://127.0.0.1:12345").rstrip("/")
TIMEOUT_SECONDS = 15

RUHUA_HOTEL = "\u5982\u534e\u9152\u5e97"
QUALIFIED_RUHUA_HOTEL = (
    "\u5e7f\u4e1c\u7701\u4e2d\u5c71\u5e02"
    "\u7535\u5b50\u79d1\u6280\u5927\u5b66\u4e2d\u5c71\u5b66\u9662"
    "\u9644\u8fd1\u5982\u534e\u9152\u5e97"
)
NAVIGATE_TO = "\u5bfc\u822a\u5230"


def _post_json(path, body, device_id):
    response = requests.post(
        f"{BASE_URL}{path}",
        json=body,
        headers={"X-Device-ID": device_id},
        timeout=TIMEOUT_SECONDS,
    )
    response.raise_for_status()
    return response.json()


def _cleanup_navigation(device_id):
    try:
        _post_json("/exit_navigation", {"device_id": device_id}, device_id)
    except requests.RequestException:
        pass


def test_live_qualified_ruhua_hotel_can_navigate_from_default_campus():
    device_id = "live-test-ruhua-qualified"
    _cleanup_navigation(device_id)

    try:
        data = _post_json(
            "/ai",
            {
                "device_id": device_id,
                "message": f"{NAVIGATE_TO}{QUALIFIED_RUHUA_HOTEL}",
            },
            device_id,
        )

        assert data["ok"] is True
        assert data["intent"] == "navigation.start"
        assert data["navigation"]["active"] is True
        assert data["navigation"]["remaining_distance"] is not None
        assert data["navigation"]["remaining_distance"] <= 100
    finally:
        _cleanup_navigation(device_id)


def test_live_short_ruhua_hotel_should_navigate_from_default_campus():
    device_id = "live-test-ruhua-short"
    _cleanup_navigation(device_id)

    try:
        data = _post_json(
            "/ai",
            {"device_id": device_id, "message": f"{NAVIGATE_TO}{RUHUA_HOTEL}"},
            device_id,
        )

        assert data["ok"] is True, data
        assert data["intent"] == "navigation.start"
        assert data["navigation"]["active"] is True
        assert data["navigation"]["remaining_distance"] is not None
        assert data["navigation"]["remaining_distance"] <= 3000
    finally:
        _cleanup_navigation(device_id)
