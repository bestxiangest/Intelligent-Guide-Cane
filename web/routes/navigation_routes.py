from flask import Blueprint, jsonify, request

from services import navigation_service
from utils.validators import get_device_id, require_json_dict

navigation_bp = Blueprint("navigation", __name__)


@navigation_bp.route("/navigation_update", methods=["POST"])
def navigation_update():
    data = require_json_dict(request)
    device_id = get_device_id(request, data)
    payload = navigation_service.update_navigation(device_id)
    status = 200 if payload.get("ok") else 404
    return jsonify(payload), status


@navigation_bp.route("/exit_navigation", methods=["POST"])
def exit_navigation():
    data = require_json_dict(request)
    device_id = get_device_id(request, data)
    payload = navigation_service.exit_navigation(device_id)
    return jsonify(payload), 200
