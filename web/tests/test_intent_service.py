from models.intent import IntentData
from services.intent_service import fast_intent, parse_and_validate_intent


def test_fast_intent_greeting_skips_llm():
    intent = fast_intent("你好", {})
    assert intent.intent == "chat.normal"
    assert intent.reply == "我在呢，请说导航、天气或者帮助。"


def test_fast_intent_navigation_start():
    intent = fast_intent("导航到图书馆", {})
    assert intent.intent == "navigation.start"
    assert intent.arguments["destination"] == "图书馆"


def test_fast_intent_navigation_exit():
    intent = fast_intent("停止导航", {})
    assert intent.intent == "navigation.exit"


def test_fast_intent_weather_query():
    intent = fast_intent("今天天气怎么样", {})
    assert intent.intent == "weather.query"
    assert intent.arguments == {"city": "current", "date": "today"}


def test_fast_intent_navigation_status():
    intent = fast_intent("还有多远", {})
    assert intent.intent == "navigation.status"


def test_parse_legal_json():
    raw = {
        "intent": "weather.query",
        "confidence": 0.9,
        "arguments": {"city": "current", "date": "today"},
        "reply": "",
        "safety_level": "normal",
    }
    intent = parse_and_validate_intent(raw)
    assert intent.intent == "weather.query"
    assert intent.arguments["date"] == "today"


def test_parse_illegal_json_falls_back_unknown():
    intent = parse_and_validate_intent("不是 JSON")
    assert intent.intent == "unknown"


def test_parse_missing_fields_falls_back_unknown():
    intent = parse_and_validate_intent({"intent": "chat.normal"})
    assert intent.intent == "unknown"


def test_parse_unknown_intent_falls_back_unknown():
    intent = parse_and_validate_intent(
        {
            "intent": "run.anything",
            "confidence": 0.9,
            "arguments": {},
            "reply": "",
            "safety_level": "normal",
        }
    )
    assert intent.intent == "unknown"


def test_parse_low_confidence_falls_back_unknown():
    intent = parse_and_validate_intent(
        {
            "intent": "chat.normal",
            "confidence": 0.2,
            "arguments": {},
            "reply": "你好",
            "safety_level": "normal",
        }
    )
    assert intent.intent == "unknown"
