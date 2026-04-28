from utils.validators import get_device_id


def test_device_id_header_precedence(app):
    with app.test_request_context(
        "/ai",
        method="POST",
        json={"device_id": "body-device"},
        headers={"X-Device-ID": "header-device"},
    ):
        from flask import request

        assert get_device_id(request, request.get_json()) == "header-device"


def test_device_id_body_second(app):
    with app.test_request_context("/ai", method="POST", json={"device_id": "body-device"}):
        from flask import request

        assert get_device_id(request, request.get_json()) == "body-device"


def test_device_id_remote_addr_fallback(app):
    with app.test_request_context("/ai", method="POST", json={}, environ_base={"REMOTE_ADDR": "10.0.0.8"}):
        from flask import request

        assert get_device_id(request, request.get_json()) == "10.0.0.8"
