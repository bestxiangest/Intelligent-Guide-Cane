from flask import Blueprint, jsonify, request

from services import location_service, response_service
from utils.validators import get_device_id, require_json_dict

gps_bp = Blueprint("gps", __name__)


@gps_bp.route("/gps", methods=["POST"])
def receive_gps_data():
    data = require_json_dict(request)
    device_id = get_device_id(request, data)
    latitude = data.get("latitude")
    longitude = data.get("longitude")
    if latitude is None or longitude is None:
        payload = response_service.api_response(
            device_id=device_id,
            intent="location.update",
            response="缺少经纬度。",
            ok=False,
            error={"code": "missing_latitude_or_longitude"},
        )
        return jsonify(payload), 400
    try:
        latitude = float(latitude)
        longitude = float(longitude)
    except (TypeError, ValueError):
        payload = response_service.api_response(
            device_id=device_id,
            intent="location.update",
            response="经纬度格式无效。",
            ok=False,
            error={"code": "invalid_latitude_or_longitude"},
        )
        return jsonify(payload), 400

    info = location_service.update_location(device_id, latitude, longitude)
    payload = response_service.none_response(
        device_id=device_id,
        intent="location.update",
        response="GPS data received successfully",
        extra={
            "message": "GPS data received successfully",
            "city": info["city"],
            "district": info["district"],
            "formatted_address": info["formatted_address"],
            "location": info,
        },
    )
    return jsonify(payload), 200
