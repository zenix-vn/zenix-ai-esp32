"""Chon key/model theo goi cuoc hoac key rieng."""
import time

import pytest

from app import db, keys
from app.runtime import resolve


def _user(name, role="user", plan_id=None, expires=None):
    return db.execute("INSERT INTO users(username, password_hash, role, created_at, plan_id, plan_expires_at) "
                      "VALUES(?,?,?,?,?,?)", name, "x", role, time.time(), plan_id, expires)


def _plan(name, provider="openrouter", model="", key="", **extra):
    cols = {"name": name, "llm_provider": provider, "llm_model": model,
            "api_key": keys.encrypt(key) if key else "", "created_at": time.time(), **extra}
    return db.execute(f"INSERT INTO plans({', '.join(cols)}) VALUES({', '.join('?' * len(cols))})", *cols.values())


@pytest.fixture(autouse=True)
def system_defaults():
    keys.set_system_key("sk-or-system")
    db.set_setting("default_llm_model", "google/gemini-2.5-flash")
    db.set_setting("default_tts_provider", "")


def agent(owner, provider="", model=""):
    return {"user_id": owner, "llm_provider": provider, "llm_model": model, "language": "vi"}


def test_plan_without_key_uses_system_key():
    basic = _plan("Basic", model="openai/gpt-4o-mini", stt_model="openai/whisper-large-v3-turbo")
    u = _user("rt-basic", plan_id=basic)
    rt = resolve(agent(u))
    assert (rt.llm_provider, rt.llm_key, rt.llm_model) == ("openrouter", "sk-or-system", "openai/gpt-4o-mini")
    assert (rt.stt_backend, rt.stt_key, rt.stt_model) == ("openrouter", "sk-or-system", "openai/whisper-large-v3-turbo")
    assert rt.tts_provider == "edge"


def test_plan_with_own_key_and_direct_provider():
    pro = _plan("Pro DeepSeek", provider="deepseek", model="deepseek-chat", key="sk-plan-deepseek")
    u = _user("rt-pro", plan_id=pro)
    rt = resolve(agent(u))
    assert (rt.llm_provider, rt.llm_key, rt.llm_model) == ("deepseek", "sk-plan-deepseek", "deepseek-chat")
    assert rt.stt_key == "sk-or-system"  # giong noi cua goi: qua OpenRouter he thong


def test_expired_or_missing_plan_is_rejected_for_users_but_not_admin():
    p = _plan("Old")
    expired = _user("rt-expired", plan_id=p, expires=time.time() - 60)
    with pytest.raises(keys.NoPlan):
        resolve(agent(expired))
    with pytest.raises(keys.NoPlan):
        resolve(agent(_user("rt-none")))
    admin = _user("rt-admin", role="admin")
    assert resolve(agent(admin)).llm_model == "google/gemini-2.5-flash"
    assert resolve(agent(None)).llm_key == "sk-or-system"  # thiet bi chua gan tac nhan


def test_own_key_bypasses_plan():
    p = _plan("Basic2", model="openai/gpt-4o-mini")
    u = _user("rt-own", plan_id=p)
    with pytest.raises(keys.KeyMissing):
        resolve(agent(u, "openai", "gpt-4o-mini"))
    keys.set_user_key(u, "openai", "sk-user-openai")
    rt = resolve(agent(u, "openai", "gpt-4.1-mini"))
    assert (rt.llm_provider, rt.llm_key, rt.llm_model) == ("openai", "sk-user-openai", "gpt-4.1-mini")
    # Khong co key OpenRouter rieng -> giong noi goi thang OpenAI bang key cua nguoi dung.
    assert (rt.stt_backend, rt.stt_key) == ("openai", "sk-user-openai")


def test_own_key_user_without_plan_and_without_audio_key():
    u = _user("rt-groq")
    keys.set_user_key(u, "groq", "sk-user-groq")
    rt = resolve(agent(u, "groq", "llama-3.3-70b-versatile"))
    assert (rt.stt_backend, rt.stt_model) == ("groq", "whisper-large-v3-turbo")
    assert rt.tts_provider == "edge"
    u2 = _user("rt-deepseek-only")
    keys.set_user_key(u2, "deepseek", "sk-ds")
    with pytest.raises(keys.KeyMissing):  # khong co cach nao nhan dang giong noi
        resolve(agent(u2, "deepseek", "deepseek-chat"))
