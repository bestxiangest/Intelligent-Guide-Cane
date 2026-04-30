import re
from typing import Any

from clients import qwen_client
from config import INTENT_CONFIDENCE_THRESHOLD
from models.intent import (
    ALLOWED_INTENTS,
    ALLOWED_SAFETY_LEVELS,
    ALLOWED_WEATHER_DATES,
    IntentData,
    unknown_intent,
)
from services import conversation_service, weather_service
from utils.json_utils import loads_json_object

GREETING_TEXTS = {"你好", "您好", "在吗", "小助手", "喂"}
HELP_TEXTS = {"帮助", "怎么用", "你能做什么"}
EXIT_EXPLICIT = {"停止导航", "取消导航", "退出导航", "结束导航"}
EXIT_SHORT = {"算了", "不去了", "停止", "取消", "退出"}
NAV_PREFIXES = ("帮我导航到", "导航到", "带我去", "我要去", "去")
WEATHER_KEYWORDS = ("天气", "下雨", "温度", "冷不冷", "热不热", "气温")
NAV_UPDATE_TEXTS = ("下一步", "继续导航", "现在怎么走", "怎么走")
NAV_STATUS_TEXTS = ("还有多远", "到哪了", "快到了吗")
JOKE_TEXTS = ("讲个笑话", "讲笑话", "说个笑话", "来个笑话", "笑话")
STORY_TEXTS = ("讲个故事", "讲故事", "说个故事", "来个故事", "故事")
CHAT_TEXTS = ("陪我聊天", "聊聊天", "陪我说话", "和我聊天", "安慰我", "鼓励我")

JOKE_REPLY = "当然。路灯为什么不睡觉？因为它一闭眼，整条街就黑了。"
STORY_REPLY = "当然。从前有颗小星星迷了路，听见脚步声后，跟着勇气回了家。"
CHAT_REPLY = "我在呢。你可以慢慢说，我会陪你聊，也能帮你导航和查天气。"


def fast_intent(text: str, device_context: dict[str, Any] | None = None) -> IntentData | None:
    normalized = _normalize_text(text)
    if not normalized:
        return unknown_intent(source="fast")

    navigation_active = bool((device_context or {}).get("navigation_active"))

    if normalized in GREETING_TEXTS:
        return IntentData(
            intent="chat.normal",
            confidence=1.0,
            arguments={},
            reply="我在呢，请说导航、天气或者帮助。",
            safety_level="normal",
            source="fast",
        )

    if any(phrase in normalized for phrase in JOKE_TEXTS):
        return IntentData(
            intent="chat.normal",
            confidence=0.98,
            arguments={},
            reply=JOKE_REPLY,
            safety_level="normal",
            source="fast",
        )

    if any(phrase in normalized for phrase in STORY_TEXTS):
        return IntentData(
            intent="chat.normal",
            confidence=0.96,
            arguments={},
            reply=STORY_REPLY,
            safety_level="normal",
            source="fast",
        )

    if any(phrase in normalized for phrase in CHAT_TEXTS):
        return IntentData(
            intent="chat.normal",
            confidence=0.94,
            arguments={},
            reply=CHAT_REPLY,
            safety_level="normal",
            source="fast",
        )

    if normalized in HELP_TEXTS:
        return IntentData(intent="system.help", confidence=1.0, source="fast")

    if normalized in EXIT_EXPLICIT or (navigation_active and normalized in EXIT_SHORT):
        return IntentData(intent="navigation.exit", confidence=1.0, source="fast")

    for prefix in NAV_PREFIXES:
        if normalized.startswith(prefix) and len(normalized) > len(prefix):
            destination = normalized[len(prefix) :].strip("，。,. ")
            if destination:
                return IntentData(
                    intent="navigation.start",
                    confidence=0.96,
                    arguments={"destination": destination},
                    source="fast",
                )

    if any(keyword in normalized for keyword in WEATHER_KEYWORDS):
        return IntentData(
            intent="weather.query",
            confidence=0.9,
            arguments=_parse_weather_args(normalized),
            source="fast",
        )

    if any(phrase in normalized for phrase in NAV_UPDATE_TEXTS):
        return IntentData(intent="navigation.update", confidence=0.9, source="fast")

    if any(phrase in normalized for phrase in NAV_STATUS_TEXTS):
        return IntentData(intent="navigation.status", confidence=0.9, source="fast")

    return None


def recognize_intent(device_id: str, text: str, device_context: dict[str, Any] | None = None) -> IntentData:
    intent = fast_intent(text, device_context)
    if intent:
        return intent

    messages = conversation_service.build_messages(device_id, text)
    raw = qwen_client.call_qwen(messages)
    if raw:
        conversation_service.record_assistant(device_id, raw)
    return parse_and_validate_intent(raw)


def parse_and_validate_intent(raw: str | dict[str, Any] | None) -> IntentData:
    data = loads_json_object(raw)
    if not data:
        return unknown_intent(source="parse")

    required = {"intent", "confidence", "arguments", "reply", "safety_level"}
    if not required.issubset(set(data.keys())):
        return unknown_intent(source="validate")

    intent = str(data.get("intent", "")).strip()
    if intent not in ALLOWED_INTENTS:
        return unknown_intent(source="validate")

    try:
        confidence = float(data.get("confidence", 0.0))
    except (TypeError, ValueError):
        confidence = 0.0
    confidence = max(0.0, min(confidence, 1.0))

    arguments = data.get("arguments") if isinstance(data.get("arguments"), dict) else {}
    reply = str(data.get("reply") or "").strip()
    safety_level = str(data.get("safety_level") or "normal").strip()
    if safety_level not in ALLOWED_SAFETY_LEVELS:
        safety_level = "normal"

    validated = IntentData(
        intent=intent,
        confidence=confidence,
        arguments=arguments,
        reply=reply,
        safety_level=safety_level,
        source="llm",
    )

    if confidence < INTENT_CONFIDENCE_THRESHOLD:
        return unknown_intent(source="low_confidence")

    if intent == "navigation.start" and not str(arguments.get("destination", "")).strip():
        return unknown_intent(source="validate")

    if intent == "weather.query":
        city = str(arguments.get("city") or "current").strip() or "current"
        date = weather_service.normalize_weather_date(str(arguments.get("date") or "today"))
        if date not in ALLOWED_WEATHER_DATES:
            date = "today"
        validated.arguments = {"city": city, "date": date}

    if intent in ("chat.normal", "unknown") and not validated.reply:
        validated.reply = "我没有听明白，请再说一遍。" if intent == "unknown" else "我在呢，请说导航、天气或者帮助。"

    return validated


def _normalize_text(text: str) -> str:
    return re.sub(r"[\s，。！？!?,.、]", "", (text or "").strip())


def _parse_weather_args(text: str) -> dict:
    date = "today"
    if "后天" in text or "后日" in text:
        date = "day_after_tomorrow"
    elif "明天" in text or "明日" in text:
        date = "tomorrow"

    city = "current"
    suffix_match = re.search(r"([\u4e00-\u9fa5]{2,10}(?:市|县|区|州|盟))(?:的)?(?:天气|温度|气温)", text)
    if suffix_match:
        candidate = suffix_match.group(1)
        if candidate not in {"今天", "明天", "后天", "当前城市"}:
            city = candidate
    return {"city": city, "date": date}
