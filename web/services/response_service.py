from __future__ import annotations

from uuid import uuid4

from config import get_missing_keys
from models.responses import LOCAL_AUDIO_IDS, SPEAK_LOCAL_AUDIO, SPEAK_NONE, SPEAK_TTS_TEXT


CONFIG_LABELS = {
    "QWEATHER_API_KEY": "和风天气 API Key",
    "AL_API_KEY": "DashScope API Key",
    "AMAP_API_KEY": "高德地图 API Key",
}


def api_response(
    device_id: str,
    intent: str,
    response: str,
    ok: bool = True,
    speak_mode: str = SPEAK_TTS_TEXT,
    speak_text: str | None = None,
    audio_id: str = "",
    audio_url: str = "",
    navigation: dict | None = None,
    error: dict | None = None,
    extra: dict | None = None,
) -> dict:
    payload = {
        "ok": ok,
        "request_id": str(uuid4()),
        "device_id": device_id,
        "intent": intent,
        "response": response,
        "speak": {
            "mode": speak_mode,
            "text": response if speak_text is None else speak_text,
            "audio_id": audio_id,
            "audio_url": audio_url,
        },
        "navigation": navigation
        if navigation is not None
        else {
            "active": False,
            "destination": "",
            "remaining_distance": None,
            "total_duration": None,
            "next_instruction": "",
        },
        "error": error,
    }
    if extra:
        payload.update(extra)
    return payload


def local_audio_response(
    device_id: str,
    intent: str,
    response: str,
    audio_key: str,
    ok: bool = True,
    navigation: dict | None = None,
    error: dict | None = None,
    extra: dict | None = None,
) -> dict:
    return api_response(
        device_id=device_id,
        intent=intent,
        response=response,
        ok=ok,
        speak_mode=SPEAK_LOCAL_AUDIO,
        speak_text=response,
        audio_id=LOCAL_AUDIO_IDS.get(audio_key, audio_key),
        navigation=navigation,
        error=error,
        extra=extra,
    )


def none_response(device_id: str, intent: str, response: str, extra: dict | None = None) -> dict:
    return api_response(
        device_id=device_id,
        intent=intent,
        response=response,
        speak_mode=SPEAK_NONE,
        speak_text="",
        extra=extra,
    )


def config_error(device_id: str, intent: str, *keys: str) -> dict | None:
    missing = get_missing_keys(*keys)
    if not missing:
        return None
    labels = [CONFIG_LABELS.get(key, key) for key in missing]
    message = f"服务端缺少配置：{', '.join(labels)}。请先填写本地配置。"
    return api_response(
        device_id=device_id,
        intent=intent,
        response=message,
        ok=False,
        error={"code": "missing_config", "missing_config": missing},
    )
