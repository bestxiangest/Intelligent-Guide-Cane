from clients import amap_client

RUHUA_HOTEL = "\u5982\u534e\u9152\u5e97"
CITY_RUHUA_HOTEL = "\u5e7f\u4e1c\u7701\u4e2d\u5c71\u5e02\u5982\u534e\u9152\u5e97"
ARRIVAL_INSTRUCTION = "\u5411\u5357\u6b65\u884c1\u7c73\u5230\u8fbe\u76ee\u7684\u5730"


class FakeResponse:
    def __init__(self, payload):
        self._payload = payload

    def raise_for_status(self):
        return None

    def json(self):
        return self._payload


def test_geocode_destination_returns_first_location(monkeypatch):
    captured = {}

    def fake_get(url, params, timeout):
        captured["url"] = url
        captured["params"] = params
        captured["timeout"] = timeout
        return FakeResponse(
            {
                "status": "1",
                "geocodes": [
                    {
                        "formatted_address": CITY_RUHUA_HOTEL,
                        "location": "113.390400,22.527390",
                    }
                ],
            }
        )

    monkeypatch.setattr(amap_client, "AMAP_API_KEY", "test-amap-key")
    monkeypatch.setattr(amap_client.requests, "get", fake_get)

    result = amap_client.geocode_destination(RUHUA_HOTEL)

    assert result == "113.390400,22.527390"
    assert captured["url"].endswith("/v3/geocode/geo")
    assert captured["params"]["address"] == RUHUA_HOTEL
    assert captured["params"]["output"] == "json"
    assert captured["params"]["key"] == "test-amap-key"


def test_walking_navigation_uses_default_origin_when_origin_missing(monkeypatch):
    captured = {}

    def fake_get(url, params, timeout):
        captured["url"] = url
        captured["params"] = params
        captured["timeout"] = timeout
        return FakeResponse(
            {
                "status": "1",
                "route": {
                    "paths": [
                        {
                            "distance": "1",
                            "cost": {"duration": "1"},
                            "steps": [{"instruction": ARRIVAL_INSTRUCTION}],
                        }
                    ]
                },
            }
        )

    monkeypatch.setattr(amap_client, "AMAP_API_KEY", "test-amap-key")
    monkeypatch.setattr(amap_client, "DEFAULT_LOCATION", "113.390342,22.527403")
    monkeypatch.setattr(amap_client.requests, "get", fake_get)

    result = amap_client.walking_navigation(None, "113.390400,22.527390")

    assert result == {
        "distance": 1,
        "duration": 1,
        "steps": [{"instruction": ARRIVAL_INSTRUCTION}],
        "first_instruction": ARRIVAL_INSTRUCTION,
    }
    assert captured["url"].endswith("/v5/direction/walking")
    assert captured["params"]["origin"] == "113.390342,22.527403"
    assert captured["params"]["destination"] == "113.390400,22.527390"
    assert captured["params"]["show_fields"] == "navi"
