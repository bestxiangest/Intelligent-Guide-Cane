from clients import qwen_client


def test_ai_greeting_uses_fast_intent_without_llm(client, monkeypatch):
    def fail_if_called(messages):
        raise AssertionError("LLM should not be called for greeting")

    monkeypatch.setattr(qwen_client, "call_qwen", fail_if_called)
    response = client.post("/ai", json={"message": "你好"}, headers={"X-Device-ID": "guide-cane-001"})
    assert response.status_code == 200
    data = response.get_json()
    assert data["device_id"] == "guide-cane-001"
    assert data["intent"] == "chat.normal"
    assert data["speak"]["mode"] == "local_audio"
    assert data["speak"]["audio_id"] == "hello_001"


def test_navigation_status_fast_intent_no_session(client):
    response = client.post("/ai", json={"message": "还有多远"}, headers={"X-Device-ID": "guide-cane-002"})
    assert response.status_code == 200
    data = response.get_json()
    assert data["intent"] == "navigation.status"
    assert data["response"] == "当前没有进行中的导航。"
