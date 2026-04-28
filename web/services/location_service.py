import threading
import time

from clients import amap_client
from config import DEFAULT_LOCATION, DEFAULT_WEATHER_CITY, LOCATION_TIMEOUT
from services import response_service

_locations: dict[str, dict] = {}
_lock = threading.Lock()


def update_location(device_id: str, latitude: float, longitude: float) -> dict:
    regeo = amap_client.reverse_geocode(longitude, latitude)
    info = {
        "latitude": latitude,
        "longitude": longitude,
        "city": (regeo or {}).get("city") or DEFAULT_WEATHER_CITY,
        "district": (regeo or {}).get("district", ""),
        "formatted_address": (regeo or {}).get("formatted_address", ""),
        "updated_at": time.time(),
    }
    with _lock:
        _locations[device_id] = info
    return dict(info)


def get_location(device_id: str) -> dict | None:
    with _lock:
        info = _locations.get(device_id)
        if not info:
            return None
        if time.time() - info["updated_at"] > LOCATION_TIMEOUT:
            return None
        return dict(info)


def get_origin(device_id: str) -> str:
    info = get_location(device_id)
    if not info:
        return DEFAULT_LOCATION
    return f"{info['longitude']},{info['latitude']}"


def get_city(device_id: str) -> str:
    info = get_location(device_id)
    if not info or not info.get("city"):
        return DEFAULT_WEATHER_CITY
    return info["city"]


def get_location_status(device_id: str) -> dict:
    info = get_location(device_id)
    if not info:
        return response_service.local_audio_response(
            device_id=device_id,
            intent="location.status",
            response="当前位置暂时不可用，请到开阔地点稍后再试。",
            audio_key="gps_invalid",
            ok=False,
            error={"code": "location_unavailable"},
        )
    response = f"当前位置在{info.get('city', '')}{info.get('district', '')}。"
    return response_service.api_response(
        device_id=device_id,
        intent="location.status",
        response=response,
        extra={"location": info},
    )
