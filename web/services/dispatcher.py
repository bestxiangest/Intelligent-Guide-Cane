from models.intent import IntentData
from services import location_service, navigation_service, response_service, weather_service

HELP_TEXT = "我可以帮你导航、查询天气、播报当前位置和查看设备状态。请说导航到某地，或者问今天天气。"


def dispatch_intent(device_id: str, intent_data: IntentData) -> dict:
    intent = intent_data.intent
    args = intent_data.arguments or {}

    if intent == "navigation.start":
        return navigation_service.start_navigation(device_id, str(args.get("destination", "")))
    if intent == "navigation.exit":
        return navigation_service.exit_navigation(device_id)
    if intent == "navigation.update":
        return navigation_service.update_navigation(device_id)
    if intent == "navigation.status":
        return navigation_service.get_navigation_status(device_id)
    if intent == "weather.query":
        return weather_service.query_weather(device_id, args.get("city", "current"), args.get("date", "today"))
    if intent == "location.status":
        return location_service.get_location_status(device_id)
    if intent == "device.status":
        return response_service.api_response(
            device_id=device_id,
            intent="device.status",
            response="设备在线，避障和照明由本地实时运行。",
            extra={"device_status": {"online": True}},
        )
    if intent == "system.help":
        return response_service.api_response(device_id=device_id, intent="system.help", response=HELP_TEXT)
    if intent == "chat.normal":
        reply = intent_data.reply or "我在呢，请说导航、天气或者帮助。"
        if reply == "我在呢，请说导航、天气或者帮助。":
            return response_service.local_audio_response(device_id, intent, reply, "hello")
        return response_service.api_response(device_id=device_id, intent=intent, response=reply)

    return response_service.local_audio_response(
        device_id=device_id,
        intent="unknown",
        response=intent_data.reply or "我没有听明白，请再说一遍。",
        audio_key="not_understood",
        ok=False,
    )
