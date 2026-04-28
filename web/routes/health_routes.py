from flask import Blueprint, jsonify, request

from config import get_missing_keys
from services import response_service
from utils.validators import get_device_id, require_json_dict

health_bp = Blueprint("health", __name__)


@health_bp.route("/", methods=["GET"])
def index():
    return "Hello World!"


@health_bp.route("/health", methods=["GET"])
def health():
    return jsonify({"ok": True, "service": "guide-cane-web", "missing_config": get_missing_keys()}), 200


@health_bp.route("/v1/device/status", methods=["POST"])
def device_status():
    data = require_json_dict(request)
    device_id = get_device_id(request, data)
    payload = response_service.api_response(
        device_id=device_id,
        intent="device.status",
        response="设备状态已接收。",
        extra={"device_status": data},
    )
    return jsonify(payload), 200
