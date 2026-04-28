import threading
import time

from clients import amap_client
from config import ARRIVAL_DISTANCE_METERS, NAVIGATION_TIMEOUT
from services import location_service, response_service

_sessions: dict[str, dict] = {}
_lock = threading.Lock()


def is_navigation_active(device_id: str) -> bool:
    with _lock:
        session = _sessions.get(device_id)
        if not session:
            return False
        if time.time() - session["start_time"] > NAVIGATION_TIMEOUT:
            _sessions.pop(device_id, None)
            return False
        return True


def start_navigation(device_id: str, destination: str) -> dict:
    config_error = response_service.config_error(device_id, "navigation.start", "AMAP_API_KEY")
    if config_error:
        return config_error

    destination = (destination or "").strip()
    if not destination:
        return response_service.local_audio_response(
            device_id=device_id,
            intent="navigation.start",
            response="我没有听清目的地，请再说一遍。",
            audio_key="not_understood",
            ok=False,
            error={"code": "missing_destination"},
        )

    destination_gps = None
    nav = None
    geocoded_destination = ""
    origin = location_service.get_origin(device_id)

    for candidate in _destination_candidates(device_id, destination):
        candidate_gps = amap_client.geocode_destination(candidate)
        if not candidate_gps:
            continue
        destination_gps = candidate_gps
        geocoded_destination = candidate
        nav = amap_client.walking_navigation(origin, candidate_gps)
        if nav:
            break

    if not destination_gps:
        return response_service.api_response(
            device_id=device_id,
            intent="navigation.start",
            response=f"找不到{destination}的坐标信息。",
            ok=False,
            error={"code": "destination_not_found"},
        )

    if not nav:
        return response_service.api_response(
            device_id=device_id,
            intent="navigation.start",
            response=f"无法规划到{destination}的步行路线。",
            ok=False,
            error={"code": "navigation_plan_failed"},
        )

    now = time.time()
    with _lock:
        _sessions[device_id] = {
            "destination": destination,
            "geocoded_destination": geocoded_destination,
            "destination_gps": destination_gps,
            "start_time": now,
            "last_update": now,
            "last_navigation": nav,
        }

    distance_km = nav["distance"] / 1000
    duration_min = nav["duration"] / 60
    first_instruction = nav["first_instruction"]
    text = f"路线规划完成，全程{distance_km:.1f}公里，大约需要{duration_min:.0f}分钟。{first_instruction}"
    navigation_payload = _navigation_payload(True, destination, nav["distance"], nav["duration"], first_instruction)
    return response_service.api_response(
        device_id=device_id,
        intent="navigation.start",
        response=text,
        navigation=navigation_payload,
        extra={
            "navigation_started": True,
            "destination": destination,
            "total_distance": nav["distance"],
            "total_duration": nav["duration"],
            "next_instruction": first_instruction,
        },
    )


def update_navigation(device_id: str) -> dict:
    with _lock:
        session = _sessions.get(device_id)
        if not session:
            return no_navigation(device_id, "navigation.update")
        if time.time() - session["start_time"] > NAVIGATION_TIMEOUT:
            _sessions.pop(device_id, None)
            return response_service.api_response(
                device_id=device_id,
                intent="navigation.update",
                response="导航已超时，请重新开始导航。",
                navigation=_navigation_payload(False),
                extra={"navigation_complete": True},
            )
        destination = session["destination"]
        destination_gps = session["destination_gps"]
        session["last_update"] = time.time()

    origin = location_service.get_origin(device_id)
    nav = amap_client.walking_navigation(origin, destination_gps)
    if not nav:
        return response_service.api_response(
            device_id=device_id,
            intent="navigation.update",
            response=f"无法更新到{destination}的导航路线。",
            ok=False,
            error={"code": "navigation_update_failed"},
        )

    if nav["distance"] <= ARRIVAL_DISTANCE_METERS:
        with _lock:
            _sessions.pop(device_id, None)
        return response_service.api_response(
            device_id=device_id,
            intent="navigation.update",
            response=f"恭喜您，已到达目的地{destination}，导航结束。",
            navigation=_navigation_payload(False, destination, 0, 0, ""),
            extra={"navigation_complete": True},
        )

    with _lock:
        if device_id in _sessions:
            _sessions[device_id]["last_navigation"] = nav

    instruction = nav["first_instruction"]
    navigation_payload = _navigation_payload(True, destination, nav["distance"], nav["duration"], instruction)
    return response_service.api_response(
        device_id=device_id,
        intent="navigation.update",
        response=instruction,
        navigation=navigation_payload,
        extra={
            "next_instruction": instruction,
            "remaining_distance": nav["distance"],
            "total_duration": nav["duration"],
            "navigation_complete": False,
        },
    )


def get_navigation_status(device_id: str) -> dict:
    with _lock:
        session = _sessions.get(device_id)
        if not session:
            return no_navigation(device_id, "navigation.status")
        nav = session.get("last_navigation") or {}
        destination = session.get("destination", "")

    distance = int(nav.get("distance", 0))
    duration = int(nav.get("duration", 0))
    instruction = nav.get("first_instruction", "")
    if distance:
        text = f"正在前往{destination}，剩余约{distance}米，下一步：{instruction}"
    else:
        text = f"正在前往{destination}，{instruction}"
    navigation_payload = _navigation_payload(True, destination, distance, duration, instruction)
    return response_service.api_response(
        device_id=device_id,
        intent="navigation.status",
        response=text,
        navigation=navigation_payload,
        extra={"next_instruction": instruction, "remaining_distance": distance, "total_duration": duration},
    )


def exit_navigation(device_id: str) -> dict:
    with _lock:
        session = _sessions.pop(device_id, None)
    if session:
        destination = session.get("destination", "")
        return response_service.local_audio_response(
            device_id=device_id,
            intent="navigation.exit",
            response="已退出导航。",
            audio_key="nav_exit",
            navigation=_navigation_payload(False, destination),
            extra={"navigation_exited": True},
        )
    return no_navigation(device_id, "navigation.exit")


def no_navigation(device_id: str, intent: str) -> dict:
    return response_service.local_audio_response(
        device_id=device_id,
        intent=intent,
        response="当前没有进行中的导航。",
        audio_key="no_navigation",
        ok=False,
        navigation=_navigation_payload(False),
        extra={"navigation_complete": True},
    )


def _destination_candidates(device_id: str, destination: str) -> list[str]:
    candidates = [destination]
    city = (location_service.get_city(device_id) or "").strip()
    if city and city not in destination:
        candidates.append(f"{city}{destination}")
    return list(dict.fromkeys(candidates))


def _navigation_payload(
    active: bool,
    destination: str = "",
    remaining_distance: int | None = None,
    total_duration: int | None = None,
    next_instruction: str = "",
) -> dict:
    return {
        "active": active,
        "destination": destination,
        "remaining_distance": remaining_distance,
        "total_duration": total_duration,
        "next_instruction": next_instruction,
    }
