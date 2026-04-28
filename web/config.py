import os

try:
    from local_config import *  # type: ignore # noqa: F403,F401
except ImportError:
    pass


def _read_value(name: str, default: str = "") -> str:
    env_value = os.getenv(name)
    if env_value:
        return env_value
    return str(globals().get(name, default))


def _normalize_dashscope_key(value: str) -> str:
    value = (value or "").strip()
    if value and not value.startswith("sk-"):
        return f"sk-{value}"
    return value


QWEATHER_API_KEY = _read_value("QWEATHER_API_KEY")
AL_API_KEY = _normalize_dashscope_key(_read_value("AL_API_KEY"))
AMAP_API_KEY = _read_value("AMAP_API_KEY")

DEFAULT_LOCATION = _read_value("DEFAULT_LOCATION", "120.07275000000001,30.30828611111111")
DEFAULT_WEATHER_CITY = _read_value("DEFAULT_WEATHER_CITY", "南昌市")
DEFAULT_DEVICE_ID = _read_value("DEFAULT_DEVICE_ID", "guide-cane-unknown")

QWEN_MODEL = _read_value("QWEN_MODEL", "qwen-turbo")
QWEN_TEMPERATURE = float(_read_value("QWEN_TEMPERATURE", "0.1"))
QWEN_MAX_TOKENS = int(_read_value("QWEN_MAX_TOKENS", "512"))

CONVERSATION_TIMEOUT = int(_read_value("CONVERSATION_TIMEOUT", "60"))
NAVIGATION_TIMEOUT = int(_read_value("NAVIGATION_TIMEOUT", "1800"))
LOCATION_TIMEOUT = int(_read_value("LOCATION_TIMEOUT", "180"))
DEVICE_STATUS_TIMEOUT = int(_read_value("DEVICE_STATUS_TIMEOUT", "180"))
ARRIVAL_DISTANCE_METERS = int(_read_value("ARRIVAL_DISTANCE_METERS", "20"))

INTENT_CONFIDENCE_THRESHOLD = float(_read_value("INTENT_CONFIDENCE_THRESHOLD", "0.55"))
HTTP_TIMEOUT_SECONDS = int(_read_value("HTTP_TIMEOUT_SECONDS", "8"))

CONFIG_VALUES = {
    "QWEATHER_API_KEY": QWEATHER_API_KEY,
    "AL_API_KEY": AL_API_KEY,
    "AMAP_API_KEY": AMAP_API_KEY,
}


def get_missing_keys(*keys):
    keys = keys or tuple(CONFIG_VALUES.keys())
    return [key for key in keys if not _is_configured(CONFIG_VALUES.get(key, ""))]


def _is_configured(value: str) -> bool:
    value = (value or "").strip()
    if not value:
        return False
    placeholders = ("YOUR_", "sk-YOUR_", "REPLACE_ME", "CHANGE_ME")
    return not any(value.startswith(prefix) for prefix in placeholders)
