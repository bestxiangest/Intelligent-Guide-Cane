from flask import Blueprint, jsonify, request

from services import weather_service
from utils.validators import get_device_id

weather_bp = Blueprint("weather", __name__)


@weather_bp.route("/weather", methods=["GET"])
def read_weather():
    device_id = get_device_id(request, None)
    city = request.args.get("city", "current").strip() or "current"
    date = weather_service.normalize_weather_date(request.args.get("date", "today"))
    payload = weather_service.query_weather(device_id, city, date)
    status = 200 if payload.get("ok") else 503
    return jsonify(payload), status
