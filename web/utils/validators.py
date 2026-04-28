from flask import Request

from config import DEFAULT_DEVICE_ID


def get_device_id(request: Request, body: dict | None = None) -> str:
    header_id = (request.headers.get("X-Device-ID") or "").strip()
    if header_id:
        return header_id

    body_id = ""
    if isinstance(body, dict):
        body_id = str(body.get("device_id") or "").strip()
    if body_id:
        return body_id

    return request.remote_addr or DEFAULT_DEVICE_ID


def require_json_dict(request: Request) -> dict:
    if not request.is_json:
        return {}
    data = request.get_json(silent=True)
    return data if isinstance(data, dict) else {}
