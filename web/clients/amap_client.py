import requests

from config import AMAP_API_KEY, DEFAULT_LOCATION, HTTP_TIMEOUT_SECONDS
from utils.logger import get_logger

logger = get_logger(__name__)


def reverse_geocode(longitude: float, latitude: float) -> dict | None:
    if not AMAP_API_KEY:
        return None
    try:
        response = requests.get(
            "https://restapi.amap.com/v3/geocode/regeo",
            params={
                "location": f"{longitude},{latitude}",
                "extensions": "base",
                "roadlevel": 0,
                "key": AMAP_API_KEY,
            },
            timeout=HTTP_TIMEOUT_SECONDS,
        )
        response.raise_for_status()
        data = response.json()
        if data.get("status") != "1":
            return None
        address_component = data.get("regeocode", {}).get("addressComponent", {})
        city = address_component.get("city")
        if isinstance(city, list):
            city = city[0] if city else ""
        if not city:
            city = address_component.get("province", "")
        return {
            "city": city or "",
            "district": address_component.get("district", ""),
            "formatted_address": data.get("regeocode", {}).get("formatted_address", ""),
        }
    except requests.RequestException as exc:
        logger.warning("[GPS] reverse geocode failed: %s", exc)
        return None


def geocode_destination(destination: str) -> str | None:
    if not AMAP_API_KEY:
        return None
    try:
        response = requests.get(
            "https://restapi.amap.com/v3/geocode/geo",
            params={"address": destination, "output": "json", "key": AMAP_API_KEY},
            timeout=HTTP_TIMEOUT_SECONDS,
        )
        response.raise_for_status()
        data = response.json()
        if data.get("status") == "1" and data.get("geocodes"):
            return data["geocodes"][0]["location"]
        return None
    except requests.RequestException as exc:
        logger.warning("[NAV] geocode failed: %s", exc)
        return None


def walking_navigation(origin: str | None, destination_gps: str) -> dict | None:
    if not AMAP_API_KEY:
        return None
    try:
        response = requests.get(
            "https://restapi.amap.com/v5/direction/walking",
            params={
                "isindoor": "0",
                "origin": origin or DEFAULT_LOCATION,
                "destination": destination_gps,
                "key": AMAP_API_KEY,
                "show_fields": "navi",
            },
            timeout=HTTP_TIMEOUT_SECONDS,
        )
        response.raise_for_status()
        data = response.json()
        if not (data.get("status") == "1" and data.get("route") and data["route"].get("paths")):
            return None
        path = data["route"]["paths"][0]
        steps = path.get("steps", [])
        return {
            "distance": int(path.get("distance", 0)),
            "duration": int(path.get("cost", {}).get("duration", 0)),
            "steps": steps,
            "first_instruction": steps[0].get("instruction", "暂无导航指令") if steps else "暂无导航指令",
        }
    except requests.RequestException as exc:
        logger.warning("[NAV] walking navigation failed: %s", exc)
        return None
