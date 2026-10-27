"""Chon nha cung cap / key / model cho mot luot hoi thoai, theo goi cuoc hoac key rieng.

Thu tu uu tien cho LLM:
  1. Tac nhan chon nha cung cap rieng (agent.llm_provider): key cua chinh nguoi dung (admin duoc dung
     key he thong cho OpenRouter). Khong phu thuoc goi cuoc.
  2. "Theo goi cuoc": nguon / model / key cua goi con han. Goi khong co key -> key OpenRouter he thong.
  3. Khong co goi: admin (va thiet bi chua gan tac nhan cua admin) dung mac dinh he thong; nguoi dung thuong bi tu choi.

Giong noi (STT/TTS) di qua OpenRouter voi cung key do; nguoi dung chi co key OpenAI/Groq rieng
thi STT/TTS goi truc tiep nha cung cap do. Edge TTS mien phi, khong can key.
"""
import time
from dataclasses import dataclass

from . import db, keys
from .config import DEFAULTS
from .keys import KeyMissing, NoPlan

# Model giong noi mac dinh khi goi thang nha cung cap (khong qua OpenRouter).
DIRECT_STT = {"openai": "gpt-4o-mini-transcribe", "groq": "whisper-large-v3-turbo"}
DIRECT_TTS = {"openai": ("gpt-4o-mini-tts", "alloy")}


@dataclass
class Runtime:
    llm_provider: str
    llm_key: str
    llm_model: str
    stt_backend: str      # openrouter | openai | groq
    stt_key: str
    stt_model: str
    tts_provider: str     # edge | openrouter | openai
    tts_key: str
    tts_model: str
    tts_voice: str


def active_plan(user_id: int | None) -> dict | None:
    if user_id is None:
        return None
    r = db.row(
        "SELECT p.* FROM users u JOIN plans p ON p.id = u.plan_id "
        "WHERE u.id = ? AND (u.plan_expires_at IS NULL OR u.plan_expires_at > ?)", user_id, time.time())
    if r:
        r["api_key"] = keys.decrypt(r["api_key"]) if r["api_key"] else ""
    return r


def _setting(name: str) -> str:
    return db.get_setting(f"default_{name}") or DEFAULTS[name]


def _first(*values: str) -> str:
    return next((v for v in values if v), "")


def resolve(agent: dict) -> Runtime:
    owner = agent.get("user_id")
    own_provider = agent.get("llm_provider") or ""
    plan = None if own_provider else active_plan(owner)

    # ---- LLM ----
    if own_provider:
        llm_key = keys.user_key(owner, own_provider)
        if not llm_key and own_provider == "openrouter" and keys.is_admin(owner):
            llm_key = keys.system_key()
        if not llm_key:
            raise KeyMissing(own_provider)
        llm_provider = own_provider
        llm_model = agent.get("llm_model") or (_setting("llm_model") if own_provider == "openrouter" else "")
        if not llm_model:
            raise KeyMissing(own_provider)
    elif plan:
        llm_provider = plan["llm_provider"] or "openrouter"
        llm_key = plan["api_key"] or (keys.system_key() if llm_provider == "openrouter" else "")
        llm_model = plan["llm_model"] or (_setting("llm_model") if llm_provider == "openrouter" else "")
        if not llm_key or not llm_model:
            raise KeyMissing(llm_provider)
    elif owner is None or keys.is_admin(owner):
        llm_provider, llm_key, llm_model = "openrouter", keys.system_key(), _setting("llm_model")
        if not llm_key:
            raise KeyMissing("openrouter")
    else:
        raise NoPlan()

    # ---- key OpenRouter cho giong noi ----
    if own_provider:
        audio_or_key = keys.user_key(owner, "openrouter") or (keys.system_key() if keys.is_admin(owner) else "")
    elif plan:
        audio_or_key = plan["api_key"] if plan["llm_provider"] == "openrouter" and plan["api_key"] else keys.system_key()
    else:
        audio_or_key = keys.system_key()
    plan = plan or {}

    # ---- STT ----
    stt_model_or = _first(agent.get("stt_model", ""), plan.get("stt_model", ""), _setting("stt_model"))
    if audio_or_key:
        stt = ("openrouter", audio_or_key, stt_model_or)
    else:
        direct = next((p for p in DIRECT_STT if keys.user_key(owner, p)), None)
        if not direct:
            raise KeyMissing("openrouter")
        stt = (direct, keys.user_key(owner, direct), DIRECT_STT[direct])

    # ---- TTS ----
    tts_provider = _first(agent.get("tts_provider", ""), plan.get("tts_provider", ""), _setting("tts_provider"))
    tts_model = _first(agent.get("tts_model", ""), plan.get("tts_model", ""), _setting("tts_model"))
    tts_voice = _first(agent.get("tts_voice", ""), plan.get("tts_voice", ""), db.get_setting("default_tts_voice"))
    tts_key = ""
    if tts_provider == "openrouter":
        if audio_or_key:
            tts_key = audio_or_key
        elif keys.user_key(owner, "openai"):
            tts_provider, tts_key = "openai", keys.user_key(owner, "openai")
            tts_model, default_voice = DIRECT_TTS["openai"]
            tts_voice = tts_voice if tts_voice.islower() else default_voice
        else:
            tts_provider = "edge"  # khong co key: van doc duoc bang Edge (mien phi)

    return Runtime(llm_provider, llm_key, llm_model, *stt, tts_provider, tts_key, tts_model, tts_voice)
