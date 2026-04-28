from dataclasses import asdict, dataclass, field
from typing import Any


ALLOWED_INTENTS = {
    "navigation.start",
    "navigation.exit",
    "navigation.update",
    "navigation.status",
    "weather.query",
    "chat.normal",
    "location.status",
    "device.status",
    "system.help",
    "unknown",
}

ALLOWED_SAFETY_LEVELS = {"normal", "caution", "critical"}
ALLOWED_WEATHER_DATES = {"today", "tomorrow", "day_after_tomorrow"}


@dataclass
class IntentData:
    intent: str = "unknown"
    confidence: float = 0.0
    arguments: dict[str, Any] = field(default_factory=dict)
    reply: str = "我没有听明白，请再说一遍。"
    safety_level: str = "normal"
    source: str = "unknown"

    def to_dict(self) -> dict[str, Any]:
        return asdict(self)


def unknown_intent(reply: str = "我没有听明白，请再说一遍。", source: str = "fallback") -> IntentData:
    return IntentData(
        intent="unknown",
        confidence=0.3,
        arguments={},
        reply=reply,
        safety_level="normal",
        source=source,
    )
