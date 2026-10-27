"""Luong day du: OTA -> kich hoat -> lien ket -> WebSocket (hello, MCP, nghe, tra loi bang giong)."""
import json
import math
import struct

import opuslib
import pytest
from fastapi.testclient import TestClient

from app.main import app
from app import keys, llm
from app.providers import openrouter

MAC = "7c:0c:5f:d8:fe:18"
DEV_HEADERS = {"Device-Id": MAC, "Client-Id": "11111111-2222-4333-8444-555555555555"}


@pytest.fixture(scope="module")
def client():
    with TestClient(app, base_url="https://testserver") as c:
        yield c


def ota(client):
    return client.post("/xiaozhi/ota/", headers=DEV_HEADERS,
                       json={"application": {"version": "1.0.0"}, "board": {"type": "zenix-esp32p4-m3"}}).json()


def test_unknown_device_gets_code_and_waits(client):
    body = ota(client)
    assert body["websocket"]["url"] == "wss://dev.example.com/xiaozhi/v1/"
    assert len(body["activation"]["code"]) == 6
    assert body["firmware"]["version"] == "1.0.0"
    # Cung ma cho lan hoi tiep theo.
    assert ota(client)["activation"]["code"] == body["activation"]["code"]
    assert client.post("/xiaozhi/ota/activate", headers=DEV_HEADERS).status_code == 202


def test_setup_login_agent_and_bind(client):
    assert client.get("/api/auth/state").json()["needs_setup"] is True
    assert client.post("/api/auth/setup", json={"username": "admin", "password": "secret-pass"}).status_code == 200
    assert client.post("/api/auth/setup", json={"username": "other", "password": "secret-pass"}).status_code == 409
    assert client.get("/api/auth/state").json()["user"]["role"] == "admin"

    agent = client.post("/api/agents", json={"name": "Zin", "language": "vi", "system_prompt": "Bạn là Zin.", "tts_provider": "openrouter"}).json()
    code = ota(client)["activation"]["code"]
    assert client.post("/api/devices/bind", json={"code": "000000", "agent_id": agent["id"]}).status_code == 404
    assert client.post("/api/devices/bind", json={"code": code, "agent_id": agent["id"], "name": "Bàn học"}).status_code == 200

    assert client.post("/xiaozhi/ota/activate", headers=DEV_HEADERS).status_code == 200
    assert "activation" not in ota(client)
    devices = client.get("/api/devices").json()
    assert devices[0]["name"] == "Bàn học" and devices[0]["agent_name"] == "Zin"


def test_websocket_rejects_bad_token(client):
    from starlette.websockets import WebSocketDisconnect
    with pytest.raises(WebSocketDisconnect):
        with client.websocket_connect("/xiaozhi/v1/", headers={**DEV_HEADERS, "Authorization": "Bearer nope"}) as ws:
            ws.receive_text()


def sine_opus_frames(seconds=0.6, rate=16000):
    enc = opuslib.Encoder(rate, 1, opuslib.APPLICATION_VOIP)
    n = rate * 60 // 1000
    frames = []
    for f in range(int(seconds / 0.06)):
        pcm = struct.pack(f"<{n}h", *(int(8000 * math.sin(2 * math.pi * 440 * (f * n + i) / rate)) for i in range(n)))
        frames.append(enc.encode(pcm, n))
    return frames


def test_conversation_with_device_tool(client, monkeypatch):
    calls = {"rounds": 0}

    keys.set_system_key("sk-or-system")

    async def fake_transcribe(key, model, wav, language):
        assert key == "sk-or-system"
        assert wav[:4] == b"RIFF" and len(wav) > 16000 and language == "vi"
        return "Âm lượng hiện tại là bao nhiêu?"

    async def fake_chat(provider, key, model, messages, tools=None):
        assert (provider, key, model) == ("openrouter", "sk-or-system", "google/gemini-2.5-flash")
        calls["rounds"] += 1
        if calls["rounds"] == 1:
            assert tools and tools[0]["function"]["name"] == "self_get_device_status"
            yield {"tool_calls": [{"id": "c1", "name": "self_get_device_status", "arguments": "{}"}]}
        else:
            assert messages[-1] == {"role": "tool", "tool_call_id": "c1", "content": '{"volume":70}'}
            for piece in ["Âm lượng đang là bảy mươi. ", "Bạn muốn đổi không?"]:
                yield {"text": piece}

    async def fake_speech(key, model, voice, text, instructions=""):
        return b"\x00\x00" * (24000 // 5)  # 0.2 s im lang

    monkeypatch.setattr(openrouter, "transcribe", fake_transcribe)
    monkeypatch.setattr(llm, "chat_stream", fake_chat)
    monkeypatch.setattr(openrouter, "speech_pcm", fake_speech)

    token = ota(client)["websocket"]["token"]
    with client.websocket_connect("/xiaozhi/v1/", headers={**DEV_HEADERS, "Authorization": f"Bearer {token}",
                                                           "Protocol-Version": "1"}) as ws:
        ws.send_text(json.dumps({"type": "hello", "version": 1, "features": {"mcp": True}, "transport": "websocket",
                                 "audio_params": {"format": "opus", "sample_rate": 16000, "channels": 1, "frame_duration": 60}}))
        hello = json.loads(ws.receive_text())
        assert hello["type"] == "hello" and hello["audio_params"]["sample_rate"] == 24000
        sid = hello["session_id"]

        # Server hoi tool cua thiet bi qua MCP; thiet bi gia lap tra loi.
        def answer_mcp(expect_method, result):
            msg = json.loads(ws.receive_text())
            assert msg["type"] == "mcp" and msg["payload"]["method"] == expect_method
            ws.send_text(json.dumps({"session_id": sid, "type": "mcp",
                                     "payload": {"jsonrpc": "2.0", "id": msg["payload"]["id"], "result": result}}))
            return msg["payload"]

        answer_mcp("initialize", {"protocolVersion": "2024-11-05", "capabilities": {"tools": {}}})
        answer_mcp("tools/list", {"tools": [{"name": "self.get_device_status", "description": "status",
                                             "inputSchema": {"type": "object", "properties": {}}}], "nextCursor": ""})

        ws.send_text(json.dumps({"session_id": sid, "type": "listen", "state": "start", "mode": "manual"}))
        for frame in sine_opus_frames():
            ws.send_bytes(frame)
        ws.send_text(json.dumps({"session_id": sid, "type": "listen", "state": "stop"}))

        assert json.loads(ws.receive_text()) == {"type": "stt", "text": "Âm lượng hiện tại là bao nhiêu?", "session_id": sid}
        call = answer_mcp("tools/call", {"content": [{"type": "text", "text": '{"volume":70}'}], "isError": False})
        assert call["params"]["name"] == "self.get_device_status"

        events, audio_frames = [], 0
        while True:
            msg = ws.receive()
            if msg.get("bytes"):
                audio_frames += 1
                continue
            data = json.loads(msg["text"])
            events.append((data["state"], data.get("text")))
            if data["state"] == "stop":
                break
        assert events == [("start", None), ("sentence_start", "Âm lượng đang là bảy mươi."),
                          ("sentence_start", "Bạn muốn đổi không?"), ("stop", None)]
        assert audio_frames >= 6  # 2 cau x 0.2 s / 60 ms

    did = client.get("/api/devices").json()[0]["id"]
    history = client.get(f"/api/devices/{did}/messages").json()
    assert [m["role"] for m in history] == ["user", "assistant"]
    assert history[1]["content"] == "Âm lượng đang là bảy mươi. Bạn muốn đổi không?"
