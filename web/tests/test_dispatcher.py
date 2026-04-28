from models.intent import IntentData
from services import dispatcher


def test_dispatch_navigation_start(monkeypatch):
    monkeypatch.setattr(
        dispatcher.navigation_service,
        "start_navigation",
        lambda device_id, destination: {"ok": True, "intent": "navigation.start", "response": destination},
    )
    result = dispatcher.dispatch_intent("dev-1", IntentData("navigation.start", 1, {"destination": "图书馆"}))
    assert result["intent"] == "navigation.start"
    assert result["response"] == "图书馆"


def test_dispatch_navigation_exit(monkeypatch):
    monkeypatch.setattr(
        dispatcher.navigation_service,
        "exit_navigation",
        lambda device_id: {"ok": True, "intent": "navigation.exit", "response": "已退出导航。"},
    )
    result = dispatcher.dispatch_intent("dev-1", IntentData("navigation.exit", 1, {}))
    assert result["intent"] == "navigation.exit"


def test_dispatch_weather(monkeypatch):
    monkeypatch.setattr(
        dispatcher.weather_service,
        "query_weather",
        lambda device_id, city, date: {"ok": True, "intent": "weather.query", "response": f"{city}:{date}"},
    )
    result = dispatcher.dispatch_intent("dev-1", IntentData("weather.query", 1, {"city": "current", "date": "today"}))
    assert result["response"] == "current:today"


def test_dispatch_chat_normal():
    result = dispatcher.dispatch_intent("dev-1", IntentData("chat.normal", 1, {}, "你好"))
    assert result["ok"] is True
    assert result["response"] == "你好"


def test_dispatch_unknown():
    result = dispatcher.dispatch_intent("dev-1", IntentData("unknown", 0.3, {}, "我没有听明白，请再说一遍。"))
    assert result["ok"] is False
    assert result["speak"]["audio_id"] == "not_understood_001"
