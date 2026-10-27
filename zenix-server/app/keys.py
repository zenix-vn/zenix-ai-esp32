"""API key: ma hoa khi luu (Fernet), key cua nguoi dung theo nha cung cap, va chon key/model cho moi luot goi."""
import base64
import hashlib
import time

from cryptography.fernet import Fernet, InvalidToken

from . import db
from .config import DEFAULTS, SECRET_KEY

_fernet = Fernet(base64.urlsafe_b64encode(hashlib.sha256(SECRET_KEY.encode() + b"|api-keys").digest()))


class KeyMissing(Exception):
    """Khong co API key phu hop cho nha cung cap."""


class NoPlan(Exception):
    """Nguoi dung chua co goi cuoc con han va khong dung key rieng."""


def encrypt(value: str) -> str:
    return _fernet.encrypt(value.encode()).decode()


def decrypt(value: str) -> str:
    try:
        return _fernet.decrypt(value.encode()).decode()
    except InvalidToken:
        return value  # gia tri cu luu dang ro (truoc khi co ma hoa)


def mask(key: str) -> str:
    return key[:6] + "…" + key[-4:] if len(key) > 14 else "•" * len(key)


# ---------- key he thong (admin) ----------

def system_key() -> str:
    stored = db.get_setting("openrouter_api_key")
    return decrypt(stored) if stored else ""


def set_system_key(key: str) -> None:
    db.set_setting("openrouter_api_key", encrypt(key))


# ---------- key cua nguoi dung ----------

def user_key(user_id: int | None, provider: str) -> str:
    if user_id is None:
        return ""
    r = db.row("SELECT api_key FROM user_keys WHERE user_id = ? AND provider = ?", user_id, provider)
    return decrypt(r["api_key"]) if r else ""


def set_user_key(user_id: int, provider: str, key: str) -> None:
    db.execute(
        "INSERT INTO user_keys(user_id, provider, api_key, created_at) VALUES(?, ?, ?, ?) "
        "ON CONFLICT(user_id, provider) DO UPDATE SET api_key = excluded.api_key, created_at = excluded.created_at",
        user_id, provider, encrypt(key), time.time(),
    )


def delete_user_key(user_id: int, provider: str) -> None:
    db.execute("DELETE FROM user_keys WHERE user_id = ? AND provider = ?", user_id, provider)


def user_providers(user_id: int) -> dict[str, str]:
    """provider -> key da che (de hien thi)."""
    return {r["provider"]: mask(decrypt(r["api_key"]))
            for r in db.rows("SELECT provider, api_key FROM user_keys WHERE user_id = ?", user_id)}


def is_admin(user_id: int | None) -> bool:
    if user_id is None:
        return False
    u = db.row("SELECT role FROM users WHERE id = ?", user_id)
    return bool(u and u["role"] == "admin")


def default_llm_model() -> str:
    return db.get_setting("default_llm_model") or DEFAULTS["llm_model"]
