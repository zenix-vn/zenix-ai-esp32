"""Endpoint WebSocket cho thiet bi."""
import logging

from fastapi import APIRouter, WebSocket

from . import db
from .auth import verify_device_token
from .devices import normalize_mac
from .session import Session, default_agent

log = logging.getLogger("zenix.ws")
router = APIRouter()


def _param(ws: WebSocket, header: str) -> str:
    # Mot so client (trinh duyet) khong dat duoc header -> cho phep truyen qua query.
    return ws.headers.get(header) or ws.query_params.get(header) or ""


async def device_ws(ws: WebSocket):
    mac = normalize_mac(_param(ws, "device-id"))
    token = _param(ws, "authorization").removeprefix("Bearer ").strip()
    device = db.row("SELECT * FROM devices WHERE mac = ?", mac) if mac else None
    if not device or not verify_device_token(mac, token):
        await ws.close(code=4401, reason="unauthorized")
        return
    if device["user_id"] is None:
        await ws.close(code=4403, reason="device not activated")
        return
    agent = db.row("SELECT * FROM agents WHERE id = ?", device["agent_id"]) if device["agent_id"] else None
    await ws.accept()
    log.info("device %s connected (agent %s)", mac, agent["name"] if agent else "default")
    await Session(ws, device, agent or default_agent()).run()
    log.info("device %s disconnected", mac)


for path in ("/xiaozhi/v1/", "/ws"):
    router.add_api_websocket_route(path, device_ws)
