import pytest

from services import navigation_service

RUHUA_HOTEL = "\u5982\u534e\u9152\u5e97"
ZHONGSHAN_CITY = "\u5e7f\u4e1c\u7701\u4e2d\u5c71\u5e02"
QUALIFIED_RUHUA_HOTEL = (
    "\u5e7f\u4e1c\u7701\u4e2d\u5c71\u5e02"
    "\u7535\u5b50\u79d1\u6280\u5927\u5b66\u4e2d\u5c71\u5b66\u9662"
    "\u9644\u8fd1\u5982\u534e\u9152\u5e97"
)
CITY_RUHUA_HOTEL = f"{ZHONGSHAN_CITY}{RUHUA_HOTEL}"
ARRIVAL_INSTRUCTION = "\u5411\u5357\u6b65\u884c1\u7c73\u5230\u8fbe\u76ee\u7684\u5730"
CITY_ROUTE_INSTRUCTION = "\u5411\u5357\u6b65\u884c254\u7c73\u5411\u53f3\u524d\u65b9\u884c\u8d70"


@pytest.fixture(autouse=True)
def clear_navigation_sessions():
    with navigation_service._lock:
        navigation_service._sessions.clear()
    yield
    with navigation_service._lock:
        navigation_service._sessions.clear()


def test_short_ruhua_hotel_destination_exposes_plan_failure(monkeypatch):
    monkeypatch.setattr(navigation_service.response_service, "config_error", lambda *args: None)
    monkeypatch.setattr(navigation_service.location_service, "get_origin", lambda device_id: "113.390342,22.527403")
    monkeypatch.setattr(
        navigation_service.amap_client,
        "geocode_destination",
        lambda destination: "120.000000,30.000000" if destination == RUHUA_HOTEL else None,
    )
    monkeypatch.setattr(navigation_service.amap_client, "walking_navigation", lambda origin, destination: None)

    result = navigation_service.start_navigation("test-device-ruhua-short", RUHUA_HOTEL)

    assert result["ok"] is False
    assert result["intent"] == "navigation.start"
    assert result["error"]["code"] == "navigation_plan_failed"
    assert result["navigation"]["active"] is False


def test_qualified_ruhua_hotel_destination_can_start_navigation(monkeypatch):
    calls = []

    monkeypatch.setattr(navigation_service.response_service, "config_error", lambda *args: None)
    monkeypatch.setattr(navigation_service.location_service, "get_origin", lambda device_id: "113.390342,22.527403")
    monkeypatch.setattr(
        navigation_service.amap_client,
        "geocode_destination",
        lambda destination: "113.390400,22.527390" if destination == QUALIFIED_RUHUA_HOTEL else None,
    )

    def fake_walking_navigation(origin, destination):
        calls.append((origin, destination))
        return {
            "distance": 1,
            "duration": 1,
            "steps": [{"instruction": ARRIVAL_INSTRUCTION}],
            "first_instruction": ARRIVAL_INSTRUCTION,
        }

    monkeypatch.setattr(navigation_service.amap_client, "walking_navigation", fake_walking_navigation)

    result = navigation_service.start_navigation("test-device-ruhua-qualified", QUALIFIED_RUHUA_HOTEL)

    assert result["ok"] is True
    assert result["navigation_started"] is True
    assert result["navigation"]["active"] is True
    assert result["navigation"]["destination"] == QUALIFIED_RUHUA_HOTEL
    assert result["navigation"]["remaining_distance"] == 1
    assert result["next_instruction"] == ARRIVAL_INSTRUCTION
    assert calls == [("113.390342,22.527403", "113.390400,22.527390")]


def test_short_ruhua_hotel_retries_with_current_city_after_plan_failure(monkeypatch):
    geocode_calls = []
    navigation_calls = []

    monkeypatch.setattr(navigation_service.response_service, "config_error", lambda *args: None)
    monkeypatch.setattr(navigation_service.location_service, "get_origin", lambda device_id: "113.390342,22.527403")
    monkeypatch.setattr(navigation_service.location_service, "get_city", lambda device_id: ZHONGSHAN_CITY)

    def fake_geocode(destination):
        geocode_calls.append(destination)
        return {
            RUHUA_HOTEL: "120.000000,30.000000",
            CITY_RUHUA_HOTEL: "113.390400,22.527390",
        }.get(destination)

    def fake_walking_navigation(origin, destination):
        navigation_calls.append((origin, destination))
        if destination == "113.390400,22.527390":
            return {
                "distance": 1564,
                "duration": 1251,
                "steps": [{"instruction": CITY_ROUTE_INSTRUCTION}],
                "first_instruction": CITY_ROUTE_INSTRUCTION,
            }
        return None

    monkeypatch.setattr(navigation_service.amap_client, "geocode_destination", fake_geocode)
    monkeypatch.setattr(navigation_service.amap_client, "walking_navigation", fake_walking_navigation)

    result = navigation_service.start_navigation("test-device-ruhua-city-fallback", RUHUA_HOTEL)

    assert result["ok"] is True
    assert result["navigation_started"] is True
    assert result["destination"] == RUHUA_HOTEL
    assert result["navigation"]["destination"] == RUHUA_HOTEL
    assert result["navigation"]["remaining_distance"] == 1564
    assert geocode_calls == [RUHUA_HOTEL, CITY_RUHUA_HOTEL]
    assert navigation_calls == [
        ("113.390342,22.527403", "120.000000,30.000000"),
        ("113.390342,22.527403", "113.390400,22.527390"),
    ]
