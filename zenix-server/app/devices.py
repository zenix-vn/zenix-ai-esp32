"""Dang ky thiet bi, ma kich hoat 6 so, endpoint OTA."""
import re
import secrets
import time

from fastapi import APIRouter, Request
from fastapi.responses import JSONResponse, Response

from . import db
from .auth import make_device_token
from .config import PUBLIC_URL, ws_url

router = APIRouter()

MAC_RE = re.compile(r"^[0-9a-f]{2}(:[0-9a-f]{2}){5}$")
CODE_TTL = 24 * 3600


def normalize_mac(value: str | None) -> str | None:
    mac = (value or "").strip().lower().replace("-", ":")
    return mac if MAC_RE.match(mac) else None


def activation_code_for(mac: str) -> str:
    """Ma 6 so con han cua thiet bi, hoac tao ma moi."""
    db.execute("DELETE FROM activation_codes WHERE expires_at < ?", time.time())
    existing = db.row("SELECT code FROM activation_codes WHERE mac = ?", mac)
    if existing:
        return existing["code"]
    while True:
        code = f"{secrets.randbelow(10**6):06d}"
        if not db.row("SELECT 1 FROM activation_codes WHERE code = ?", code):
            break
    db.execute("INSERT INTO activation_codes(code, mac, expires_at) VALUES(?, ?, ?)", code, mac, time.time() + CODE_TTL)
    return code


def upsert_device(mac: str, client_id: str, info: dict, ip: str) -> dict:
    board = (info.get("board") or {}).get("type") or ""
    firmware = (info.get("application") or {}).get("version") or ""
    dev = db.row("SELECT * FROM devices WHERE mac = ?", mac)
    if dev:
        db.execute(
            "UPDATE devices SET client_id = ?, board = COALESCE(NULLIF(?, ''), board), "
            "firmware = COALESCE(NULLIF(?, ''), firmware), last_ip = ?, last_seen = ? WHERE id = ?",
            client_id, board, firmware, ip, time.time(), dev["id"],
        )
    else:
        db.execute(
            "INSERT INTO devices(mac, client_id, board, firmware, last_ip, last_seen, created_at) VALUES(?,?,?,?,?,?,?)",
            mac, client_id, board, firmware, ip, time.time(), time.time(),
        )
    return db.row("SELECT * FROM devices WHERE mac = ?", mac)


def client_ip(request: Request) -> str:
    fwd = request.headers.get("x-forwarded-for")
    return fwd.split(",")[0].strip() if fwd else (request.client.host if request.client else "")


async def _ota(request: Request):
    mac = normalize_mac(request.headers.get("device-id"))
    if not mac:
        return JSONResponse({"error": "invalid Device-Id"}, status_code=400)
    try:
        info = await request.json()
    except ValueError:
        info = {}
    dev = upsert_device(mac, request.headers.get("client-id", ""), info if isinstance(info, dict) else {}, client_ip(request))

    now_ms = int(time.time() * 1000)
    body = {
        "server_time": {"timestamp": now_ms, "timezone_offset": 420},
        # Bao phien ban hien tai de thiet bi khong tu cap nhat firmware.
        "firmware": {"version": dev["firmware"] or "0", "url": ""},
        "websocket": {"url": ws_url(), "token": make_device_token(mac)},
    }
    if dev["user_id"] is None:
        code = activation_code_for(mac)
        body["activation"] = {
            "code": code,
            "message": f"{PUBLIC_URL}\n{code}",
            "challenge": mac,
            "timeout_ms": 30000,
        }
    return body


async def _activate(request: Request):
    """Thiet bi hoi lien tuc: 202 = chua lien ket, 200 = da lien ket."""
    mac = normalize_mac(request.headers.get("device-id"))
    dev = db.row("SELECT user_id FROM devices WHERE mac = ?", mac) if mac else None
    if dev and dev["user_id"] is not None:
        return {"ok": True}
    return Response(status_code=202)


# Duong dan tuong thich firmware hien tai (/xiaozhi/ota/) va duong dan ngan (/ota/).
for prefix in ("/xiaozhi/ota", "/ota"):
    router.add_api_route(prefix + "/", _ota, methods=["POST", "GET"])
    router.add_api_route(prefix + "/activate", _activate, methods=["POST"])


def bind_device(code: str, user_id: int, agent_id: int | None) -> dict | None:
    rec = db.row("SELECT mac FROM activation_codes WHERE code = ? AND expires_at >= ?", code.strip(), time.time())
    if not rec:
        return None
    db.execute("UPDATE devices SET user_id = ?, agent_id = ? WHERE mac = ?", user_id, agent_id, rec["mac"])
    db.execute("DELETE FROM activation_codes WHERE code = ?", code.strip())
    return db.row("SELECT * FROM devices WHERE mac = ?", rec["mac"])
