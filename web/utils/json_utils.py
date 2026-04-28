import json
import re
from typing import Any


def loads_json_object(raw: str | dict[str, Any] | None) -> dict[str, Any] | None:
    if isinstance(raw, dict):
        return raw
    if not raw:
        return None

    text = str(raw).strip()
    text = re.sub(r"^```(?:json)?", "", text, flags=re.IGNORECASE).strip()
    text = re.sub(r"```$", "", text).strip()

    for candidate in (text, _extract_first_object(text)):
        if not candidate:
            continue
        try:
            value = json.loads(candidate)
            if isinstance(value, dict):
                return value
        except json.JSONDecodeError:
            continue
    return None


def _extract_first_object(text: str) -> str | None:
    start = text.find("{")
    end = text.rfind("}")
    if start == -1 or end == -1 or end <= start:
        return None
    return text[start : end + 1]
