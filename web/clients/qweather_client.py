import requests

from config import HTTP_TIMEOUT_SECONDS, QWEATHER_API_KEY
from utils.logger import get_logger

logger = get_logger(__name__)


def get_city_id(city_name: str) -> str | None:
    if not QWEATHER_API_KEY:
        return None
    try:
        response = requests.get(
            "https://geoapi.qweather.com/v2/city/lookup",
            params={"location": city_name, "key": QWEATHER_API_KEY},
            timeout=HTTP_TIMEOUT_SECONDS,
        )
        response.raise_for_status()
        data = response.json()
        if data.get("code") == "200" and data.get("location"):
            return data["location"][0]["id"]
        return None
    except requests.RequestException as exc:
        logger.warning("[WEATHER] city lookup failed: %s", exc)
        return None


def get_weather_forecast(city_id: str) -> list[dict] | None:
    if not QWEATHER_API_KEY:
        return None
    try:
        response = requests.get(
            "https://devapi.qweather.com/v7/weather/3d",
            params={"location": city_id, "lang": "zh", "unit": "m", "key": QWEATHER_API_KEY},
            timeout=HTTP_TIMEOUT_SECONDS,
        )
        response.raise_for_status()
        data = response.json()
        if data.get("code") == "200" and data.get("daily"):
            return data["daily"]
        return None
    except requests.RequestException as exc:
        logger.warning("[WEATHER] forecast failed: %s", exc)
        return None
