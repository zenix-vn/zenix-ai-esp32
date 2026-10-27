"""API key cua nguoi dung, chon key/model cho tac nhan, dang ky, gia model."""
import pytest
from fastapi.testclient import TestClient

from app import db, keys, llm
from app.main import app


def test_encrypt_roundtrip_and_plaintext_fallback():
    enc = keys.encrypt("sk-secret-123")
    assert enc != "sk-secret-123" and keys.decrypt(enc) == "sk-secret-123"
    assert keys.decrypt("sk-legacy-plain") == "sk-legacy-plain"


def _user(name, role):
    return db.execute("INSERT INTO users(username, password_hash, role, created_at) VALUES(?,?,?,?)",
                      name, "x", role, db.now())


def test_price_lookup_for_direct_models():
    index = {"openai/gpt-4o-mini": {"price_in": 0.15}, "google/gemini-2.5-flash": {"price_in": 0.3}}
    assert llm._lookup_price(index, "openai/", "gpt-4o-mini-2024-07-18")["price_in"] == 0.15
    assert llm._lookup_price(index, "google/", "gemini-2.5-flash")["price_in"] == 0.3
    assert llm._lookup_price(index, "openai/", "gpt-9") is None


def test_register_login_and_save_key(monkeypatch):
    with TestClient(app, base_url="https://testserver") as c:
        if c.get("/api/auth/state").json()["needs_setup"]:
            c.post("/api/auth/setup", json={"username": "boss", "password": "boss-pass-1"})
            c.post("/api/auth/logout")
        assert c.get("/api/auth/state").json()["allow_registration"] is True
        r = c.post("/api/auth/register", json={"username": "newbie", "password": "newbie-pass"})
        assert r.status_code == 200
        me = c.get("/api/auth/state").json()["user"]
        assert me["username"] == "newbie" and me["role"] == "user"
        assert c.get("/api/settings").status_code == 403  # khong phai admin
        assert c.post("/api/auth/register", json={"username": "newbie", "password": "newbie-pass"}).status_code == 409

        async def fake_verify(provider, key):
            if key != "sk-good-key-123456":
                raise llm.ProviderError("invalid_key (401)")
        monkeypatch.setattr(llm, "verify_key", fake_verify)
        assert c.put("/api/keys/openai", json={"api_key": "sk-bad-key"}).status_code == 400
        assert c.put("/api/keys/openai", json={"api_key": "sk-good-key-123456"}).status_code == 200
        provs = {p["id"]: p for p in c.get("/api/providers").json()["providers"]}
        assert provs["openai"]["key_hint"] == "sk-goo…3456" and provs["gemini"]["key_hint"] == ""

        # Tac nhan: model rieng khong co nha cung cap -> OpenRouter; mac dinh -> trong.
        a = c.post("/api/agents", json={"name": "A", "llm_model": "openai/gpt-4o-mini"}).json()
        assert a["llm_provider"] == "openrouter"
        b = c.post("/api/agents", json={"name": "B", "llm_provider": "openai", "llm_model": "gpt-4o-mini"}).json()
        assert (b["llm_provider"], b["llm_model"]) == ("openai", "gpt-4o-mini")

        db.set_setting("allow_registration", "0")
        c.post("/api/auth/logout")
        assert c.post("/api/auth/register", json={"username": "late", "password": "late-pass-1"}).status_code == 403
        db.set_setting("allow_registration", "1")
