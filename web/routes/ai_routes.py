from flask import Blueprint, jsonify, request

from services import dispatcher, intent_service, navigation_service, response_service
from utils.validators import get_device_id, require_json_dict

ai_bp = Blueprint("ai", __name__)


@ai_bp.route("/ai", methods=["POST"])
def ai_chat():
    data = require_json_dict(request)
    device_id = get_device_id(request, data)
    text = str(data.get("message") or data.get("text") or "").strip()
    if not text:
        payload = response_service.api_response(
            device_id=device_id,
            intent="unknown",
            response="请求中缺少 message 字段。",
            ok=False,
            error={"code": "missing_message"},
        )
        return jsonify(payload), 400

    context = {"navigation_active": navigation_service.is_navigation_active(device_id)}
    intent = intent_service.recognize_intent(device_id, text, context)
    payload = dispatcher.dispatch_intent(device_id, intent)
    return jsonify(payload), 200


@ai_bp.route("/v1/intent", methods=["POST"])
def v1_intent():
    data = require_json_dict(request)
    device_id = get_device_id(request, data)
    text = str(data.get("message") or data.get("text") or "").strip()
    if not text:
        return jsonify({"ok": False, "device_id": device_id, "error": "missing message"}), 400
    context = {"navigation_active": navigation_service.is_navigation_active(device_id)}
    intent = intent_service.recognize_intent(device_id, text, context)
    return jsonify({"ok": True, "device_id": device_id, "intent": intent.to_dict()}), 200


@ai_bp.route("/v1/audio", methods=["POST"])
def v1_audio_placeholder():
    data = require_json_dict(request)
    device_id = get_device_id(request, data)
    payload = response_service.none_response(
        device_id=device_id,
        intent="audio.upload",
        response="音频接口已预留，第一阶段仍由设备端进行语音识别。",
    )
    return jsonify(payload), 200
