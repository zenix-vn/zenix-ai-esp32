"""Mat khau (scrypt), phien dang nhap (cookie ky boi itsdangerous), token thiet bi (HMAC)."""
import base64
import hashlib
import hmac
import secrets
import time

from fastapi import HTTPException, Request
from itsdangerous import BadSignature, URLSafeTimedSerializer

from . import db
from .config import SECRET_KEY

SESSION_COOKIE = "zenix_session"
SESSION_MAX_AGE = 14 * 24 * 3600
_serializer = URLSafeTimedSerializer(SECRET_KEY, salt="session")


def hash_password(password: str) -> str:
    salt = secrets.token_bytes(16)
    digest = hashlib.scrypt(password.encode(), salt=salt, n=2**14, r=8, p=1)
    return "scrypt$" + base64.b64encode(salt).decode() + "$" + base64.b64encode(digest).decode()


def verify_password(password: str, stored: str) -> bool:
    try:
        _, salt_b64, digest_b64 = stored.split("$")
        salt = base64.b64decode(salt_b64)
        expected = base64.b64decode(digest_b64)
    except ValueError:
        return False
    digest = hashlib.scrypt(password.encode(), salt=salt, n=2**14, r=8, p=1)
    return hmac.compare_digest(digest, expected)


def make_session(user_id: int) -> str:
    return _serializer.dumps({"uid": user_id})


def current_user(request: Request) -> dict:
    token = request.cookies.get(SESSION_COOKIE)
    if not token:
        raise HTTPException(401, "not_logged_in")
    try:
        data = _serializer.loads(token, max_age=SESSION_MAX_AGE)
    except BadSignature:
        raise HTTPException(401, "not_logged_in")
    user = db.row("SELECT id, username, role FROM users WHERE id = ?", data["uid"])
    if not user:
        raise HTTPException(401, "not_logged_in")
    return user


def require_admin(request: Request) -> dict:
    user = current_user(request)
    if user["role"] != "admin":
        raise HTTPException(403, "admin_only")
    return user


# Token WebSocket cho thiet bi: "<expiry>.<hmac(mac|expiry)>"
DEVICE_TOKEN_TTL = 30 * 24 * 3600


def make_device_token(mac: str) -> str:
    expiry = int(time.time()) + DEVICE_TOKEN_TTL
    sig = hmac.new(SECRET_KEY.encode(), f"{mac}|{expiry}".encode(), hashlib.sha256).hexdigest()
    return f"{expiry}.{sig}"


def verify_device_token(mac: str, token: str) -> bool:
    try:
        expiry_s, sig = token.split(".", 1)
        expiry = int(expiry_s)
    except ValueError:
        return False
    if expiry < time.time():
        return False
    expected = hmac.new(SECRET_KEY.encode(), f"{mac}|{expiry}".encode(), hashlib.sha256).hexdigest()
    return hmac.compare_digest(sig, expected)
