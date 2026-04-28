from clients import qweather_client
from config import DEFAULT_WEATHER_CITY
from models.intent import ALLOWED_WEATHER_DATES
from services import location_service, response_service

DATE_INDEX = {"today": 0, "tomorrow": 1, "day_after_tomorrow": 2}
DATE_LABEL = {"today": "今天", "tomorrow": "明天", "day_after_tomorrow": "后天"}


def normalize_weather_date(value: str | None) -> str:
    value = (value or "").strip()
    mapping = {
        "": "today",
        "today": "today",
        "今天": "today",
        "今日": "today",
        "tomorrow": "tomorrow",
        "明天": "tomorrow",
        "明日": "tomorrow",
        "day_after_tomorrow": "day_after_tomorrow",
        "后天": "day_after_tomorrow",
        "后日": "day_after_tomorrow",
    }
    return mapping.get(value, "today")


def query_weather(device_id: str, city: str | None, date: str | None) -> dict:
    config_error = response_service.config_error(device_id, "weather.query", "QWEATHER_API_KEY")
    if config_error:
        return config_error

    date = normalize_weather_date(date)
    if date not in ALLOWED_WEATHER_DATES:
        date = "today"

    city_name = (city or "current").strip()
    if city_name in ("", "current", "当前城市", "定位城市"):
        city_name = location_service.get_city(device_id) or DEFAULT_WEATHER_CITY

    city_id = qweather_client.get_city_id(city_name)
    if not city_id:
        return response_service.api_response(
            device_id=device_id,
            intent="weather.query",
            response=f"找不到{city_name}的城市信息。",
            ok=False,
            error={"code": "weather_city_not_found"},
        )

    forecast = qweather_client.get_weather_forecast(city_id)
    if not forecast:
        return response_service.api_response(
            device_id=device_id,
            intent="weather.query",
            response=f"获取{city_name}天气失败，请稍后再试。",
            ok=False,
            error={"code": "weather_fetch_failed"},
        )

    text = format_weather_response(city_name, date, forecast)
    return response_service.api_response(
        device_id=device_id,
        intent="weather.query",
        response=text,
        extra={"city": city_name, "date": date, "daily": forecast},
    )


def format_weather_response(city_name: str, date: str, forecast_data: list[dict]) -> str:
    index = DATE_INDEX.get(date, 0)
    if index >= len(forecast_data):
        index = 0
    item = forecast_data[index]
    day_label = DATE_LABEL.get(date, "今天")
    return (
        f"{city_name}{day_label}{item.get('textDay', '')}，"
        f"{item.get('tempMin', '')}到{item.get('tempMax', '')}度，"
        f"{item.get('windDirDay', '')}{item.get('windScaleDay', '')}级。"
    )
