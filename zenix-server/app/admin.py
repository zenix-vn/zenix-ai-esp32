"""REST API cho trang quan tri."""
import time

import httpx
from fastapi import APIRouter, Depends, HTTPException, Request, Response
from pydantic import BaseModel, Field

from . import db, keys, llm, runtime
from .auth import SESSION_COOKIE, SESSION_MAX_AGE, current_user, hash_password, make_session, require_admin, verify_password
from .config import DEFAULTS, PUBLIC_URL
from .devices import bind_device
from .providers import edge, openrouter
from .session import ONLINE

router = APIRouter(prefix="/api")


# ---------- dang nhap ----------

class Credentials(BaseModel):
    username: str = Field(min_length=3, max_length=40)
    password: str = Field(min_length=8, max_length=200)


def _login_response(resp: Response, user_id: int) -> dict:
    resp.set_cookie(SESSION_COOKIE, make_session(user_id), max_age=SESSION_MAX_AGE, httponly=True,
                    samesite="lax", secure=PUBLIC_URL.startswith("https"))
    return {"ok": True}


@router.get("/auth/state")
def auth_state(request: Request):
    needs_setup = db.row("SELECT COUNT(*) AS n FROM users")["n"] == 0
    try:
        user = current_user(request)
    except HTTPException:
        user = None
    return {"needs_setup": needs_setup, "user": user, "allow_registration": registration_open()}


def registration_open() -> bool:
    return db.get_setting("allow_registration", "1") == "1"


@router.post("/auth/register")
def register(body: Credentials, resp: Response):
    """Nguoi dung tu dang ky (vai tro 'user'). Admin co the tat trong Cai dat."""
    if db.row("SELECT COUNT(*) AS n FROM users")["n"] == 0:
        raise HTTPException(409, "needs_setup")
    if not registration_open():
        raise HTTPException(403, "registration_closed")
    if db.row("SELECT 1 FROM users WHERE username = ?", body.username.strip()):
        raise HTTPException(409, "username_taken")
    uid = db.execute("INSERT INTO users(username, password_hash, role, created_at) VALUES(?,?,?,?)",
                     body.username.strip(), hash_password(body.password), "user", time.time())
    return _login_response(resp, uid)


@router.post("/auth/setup")
def setup(body: Credentials, resp: Response):
    """Tao tai khoan quan tri dau tien. Chi dung duoc khi chua co ai."""
    if db.row("SELECT COUNT(*) AS n FROM users")["n"] > 0:
        raise HTTPException(409, "already_setup")
    uid = db.execute("INSERT INTO users(username, password_hash, role, created_at) VALUES(?,?,?,?)",
                     body.username.strip(), hash_password(body.password), "admin", time.time())
    return _login_response(resp, uid)


@router.post("/auth/login")
def login(body: Credentials, resp: Response):
    user = db.row("SELECT * FROM users WHERE username = ?", body.username.strip())
    if not user or not verify_password(body.password, user["password_hash"]):
        time.sleep(0.5)
        raise HTTPException(401, "invalid_credentials")
    return _login_response(resp, user["id"])


@router.post("/auth/logout")
def logout(resp: Response):
    resp.delete_cookie(SESSION_COOKIE)
    return {"ok": True}


# ---------- nguoi dung (admin) ----------

@router.get("/users")
def list_users(_=Depends(require_admin)):
    return db.rows(
        "SELECT u.id, u.username, u.role, u.created_at, u.plan_id, u.plan_expires_at, p.name AS plan_name, "
        "(SELECT COUNT(*) FROM devices d WHERE d.user_id = u.id) AS device_count "
        "FROM users u LEFT JOIN plans p ON p.id = u.plan_id ORDER BY u.id")


class UserUpdate(BaseModel):
    role: str | None = Field(default=None, pattern="^(user|admin)$")
    plan_id: int | None = None          # 0 = bo goi
    plan_expires_at: float | None = None  # 0 = khong het han


@router.put("/users/{uid}")
def update_user(uid: int, body: UserUpdate, me=Depends(require_admin)):
    if not db.row("SELECT 1 FROM users WHERE id = ?", uid):
        raise HTTPException(404, "user_not_found")
    if body.role is not None:
        if uid == me["id"] and body.role != "admin":
            raise HTTPException(400, "cannot_demote_self")
        db.execute("UPDATE users SET role = ? WHERE id = ?", body.role, uid)
    if body.plan_id is not None:
        if body.plan_id and not db.row("SELECT 1 FROM plans WHERE id = ?", body.plan_id):
            raise HTTPException(404, "plan_not_found")
        db.execute("UPDATE users SET plan_id = ? WHERE id = ?", body.plan_id or None, uid)
    if body.plan_expires_at is not None:
        db.execute("UPDATE users SET plan_expires_at = ? WHERE id = ?", body.plan_expires_at or None, uid)
    return {"ok": True}


class NewUser(Credentials):
    role: str = Field(default="user", pattern="^(user|admin)$")


@router.post("/users")
def create_user(body: NewUser, _=Depends(require_admin)):
    if db.row("SELECT 1 FROM users WHERE username = ?", body.username.strip()):
        raise HTTPException(409, "username_taken")
    uid = db.execute("INSERT INTO users(username, password_hash, role, created_at) VALUES(?,?,?,?)",
                     body.username.strip(), hash_password(body.password), body.role, time.time())
    return {"id": uid}


@router.delete("/users/{uid}")
def delete_user(uid: int, me=Depends(require_admin)):
    if uid == me["id"]:
        raise HTTPException(400, "cannot_delete_self")
    db.execute("DELETE FROM users WHERE id = ?", uid)
    return {"ok": True}


class PasswordChange(BaseModel):
    old_password: str
    new_password: str = Field(min_length=8, max_length=200)


@router.post("/auth/password")
def change_password(body: PasswordChange, me=Depends(current_user)):
    user = db.row("SELECT password_hash FROM users WHERE id = ?", me["id"])
    if not verify_password(body.old_password, user["password_hash"]):
        raise HTTPException(400, "invalid_credentials")
    db.execute("UPDATE users SET password_hash = ? WHERE id = ?", hash_password(body.new_password), me["id"])
    return {"ok": True}


# ---------- cai dat he thong (admin) ----------

SETTING_KEYS = ["default_llm_model", "default_stt_model", "default_tts_provider", "default_tts_model", "default_tts_voice"]


@router.get("/settings")
def get_settings(_=Depends(require_admin)):
    out = {k: db.get_setting(k) for k in SETTING_KEYS}
    key = keys.system_key()
    out["openrouter_api_key_set"] = bool(key)
    out["openrouter_api_key_hint"] = keys.mask(key) if key else ""
    out["allow_registration"] = registration_open()
    out["defaults"] = DEFAULTS
    return out


class SettingsUpdate(BaseModel):
    openrouter_api_key: str | None = None
    allow_registration: bool | None = None
    default_llm_model: str | None = None
    default_stt_model: str | None = None
    default_tts_provider: str | None = None
    default_tts_model: str | None = None
    default_tts_voice: str | None = None


@router.put("/settings")
async def put_settings(body: SettingsUpdate, _=Depends(require_admin)):
    data = body.model_dump(exclude_none=True)
    if data.get("openrouter_api_key"):
        try:
            await openrouter.check_key(data["openrouter_api_key"].strip())
        except openrouter.ProviderError as e:
            raise HTTPException(400, str(e))
        keys.set_system_key(data.pop("openrouter_api_key").strip())
    data.pop("openrouter_api_key", None)
    if "allow_registration" in data:
        db.set_setting("allow_registration", "1" if data.pop("allow_registration") else "0")
    for k, v in data.items():
        db.set_setting(k, v.strip())
    return {"ok": True}


@router.get("/settings/key-info")
async def key_info(_=Depends(require_admin)):
    key = keys.system_key()
    if not key:
        return {"set": False}
    try:
        return {"set": True, **(await openrouter.check_key(key))}
    except openrouter.ProviderError as e:
        return {"set": True, "error": str(e)}


@router.get("/models/{kind}")
async def models(kind: str, provider: str = "openrouter", plan_id: int | None = None, user=Depends(current_user)):
    if kind not in ("llm", "stt", "tts") or provider not in llm.PROVIDERS:
        raise HTTPException(404)
    try:
        if kind == "llm":
            key = keys.user_key(user["id"], provider)
            if plan_id and user["role"] == "admin":  # admin chon model cho goi: dung key cua goi
                plan = db.row("SELECT api_key FROM plans WHERE id = ?", plan_id)
                key = (keys.decrypt(plan["api_key"]) if plan and plan["api_key"] else "") or key
            return await llm.list_llm_models(provider, key)
        return await openrouter.list_models(kind)
    except openrouter.ProviderError as e:
        raise HTTPException(400, str(e))
    except Exception as e:
        raise HTTPException(502, f"provider_unreachable: {e}")


# ---------- goi cuoc (admin) ----------

PROVIDER_PATTERN = "^(openrouter|openai|gemini|deepseek|groq)$"


class PlanIn(BaseModel):
    name: str = Field(min_length=1, max_length=60)
    description: str = Field(default="", max_length=500)
    price_label: str = Field(default="", max_length=60)
    llm_provider: str = Field(default="openrouter", pattern=PROVIDER_PATTERN)
    llm_model: str = ""
    api_key: str | None = None      # None = giu nguyen; "" = xoa (dung key he thong)
    stt_model: str = ""
    tts_provider: str = Field(default="", pattern="^(|openrouter|edge)$")
    tts_model: str = ""
    tts_voice: str = ""


def _plan_view(p: dict) -> dict:
    key = keys.decrypt(p["api_key"]) if p["api_key"] else ""
    out = {k: v for k, v in p.items() if k != "api_key"}
    out["api_key_hint"] = keys.mask(key) if key else ""
    return out


@router.get("/plans")
def list_plans(_=Depends(require_admin)):
    plans = db.rows("SELECT p.*, (SELECT COUNT(*) FROM users u WHERE u.plan_id = p.id) AS user_count FROM plans p ORDER BY p.id")
    return [_plan_view(p) for p in plans]


async def _plan_cols(body: PlanIn, existing: dict | None) -> dict:
    cols = body.model_dump(exclude={"api_key"})
    cols["llm_model"] = cols["llm_model"].strip()
    if body.api_key is None:
        cols["api_key"] = existing["api_key"] if existing else ""
    elif body.api_key.strip():
        try:
            await llm.verify_key(body.llm_provider, body.api_key.strip())
        except openrouter.ProviderError as e:
            raise HTTPException(400, str(e))
        cols["api_key"] = keys.encrypt(body.api_key.strip())
    else:
        cols["api_key"] = ""
    # Goi khong co key rieng chi dung duoc OpenRouter (key he thong).
    if not cols["api_key"] and body.llm_provider != "openrouter":
        raise HTTPException(400, "plan_key_required")
    if body.llm_provider != "openrouter" and not cols["llm_model"]:
        raise HTTPException(400, "plan_model_required")
    return cols


@router.post("/plans")
async def create_plan(body: PlanIn, _=Depends(require_admin)):
    cols = await _plan_cols(body, None)
    pid = db.execute(f"INSERT INTO plans(created_at, {', '.join(cols)}) VALUES(?, {', '.join('?' * len(cols))})",
                     time.time(), *cols.values())
    return _plan_view(db.row("SELECT * FROM plans WHERE id = ?", pid))


@router.get("/plans/{pid}")
def get_plan(pid: int, _=Depends(require_admin)):
    p = db.row("SELECT * FROM plans WHERE id = ?", pid)
    if not p:
        raise HTTPException(404, "plan_not_found")
    return _plan_view(p)


@router.put("/plans/{pid}")
async def update_plan(pid: int, body: PlanIn, _=Depends(require_admin)):
    existing = db.row("SELECT * FROM plans WHERE id = ?", pid)
    if not existing:
        raise HTTPException(404, "plan_not_found")
    cols = await _plan_cols(body, existing)
    db.execute(f"UPDATE plans SET {', '.join(f'{k} = ?' for k in cols)} WHERE id = ?", *cols.values(), pid)
    return _plan_view(db.row("SELECT * FROM plans WHERE id = ?", pid))


@router.delete("/plans/{pid}")
def delete_plan(pid: int, _=Depends(require_admin)):
    db.execute("UPDATE users SET plan_id = NULL WHERE plan_id = ?", pid)
    db.execute("DELETE FROM plans WHERE id = ?", pid)
    return {"ok": True}


# ---------- nha cung cap & API key cua nguoi dung ----------

@router.get("/providers")
def providers(user=Depends(current_user)):
    mine = keys.user_providers(user["id"])
    default_model = keys.default_llm_model()
    plan = runtime.active_plan(user["id"])
    expires = db.row("SELECT plan_expires_at FROM users WHERE id = ?", user["id"])["plan_expires_at"]
    sys_default = lambda k: db.get_setting(f"default_{k}") or DEFAULTS[k]  # noqa: E731
    # Gia tri ma tac nhan "theo goi / mac dinh" se dung (de hien goi y tren giao dien).
    voice_defaults = {k: (plan or {}).get(k) or sys_default(k) for k in ("stt_model", "tts_provider", "tts_model")}
    voice_defaults["tts_voice"] = (plan or {}).get("tts_voice") or db.get_setting("default_tts_voice")
    if plan:
        default_model = plan["llm_model"] or default_model
    return {
        "default_model": default_model,
        "voice_defaults": voice_defaults,
        "plan": {k: plan[k] for k in ("id", "name", "description", "price_label", "llm_provider", "llm_model")}
                | {"expires_at": expires} if plan else None,
        "system_key": bool(keys.system_key()),
        "is_admin": user["role"] == "admin",
        "providers": [{"id": pid, "name": p["name"], "keys_url": p["keys_url"],
                       "key_hint": mine.get(pid, "")} for pid, p in llm.PROVIDERS.items()],
    }


class KeyIn(BaseModel):
    api_key: str = Field(min_length=8, max_length=400)


@router.put("/keys/{provider}")
async def save_key(provider: str, body: KeyIn, user=Depends(current_user)):
    if provider not in llm.PROVIDERS:
        raise HTTPException(404)
    try:
        await llm.verify_key(provider, body.api_key.strip())
    except openrouter.ProviderError as e:
        raise HTTPException(400, str(e))
    except httpx.HTTPError as e:
        raise HTTPException(502, f"provider_unreachable: {e}")
    keys.set_user_key(user["id"], provider, body.api_key.strip())
    return {"ok": True}


@router.delete("/keys/{provider}")
def delete_key(provider: str, user=Depends(current_user)):
    keys.delete_user_key(user["id"], provider)
    return {"ok": True}


@router.get("/voices/edge")
def edge_voices(_=Depends(current_user)):
    return edge.VOICES


# ---------- tac nhan ----------

class AgentIn(BaseModel):
    name: str = Field(min_length=1, max_length=60)
    language: str = Field(default="vi", pattern="^(vi|en)$")
    system_prompt: str = Field(default="", max_length=4000)
    llm_provider: str = Field(default="", pattern="^(|openrouter|openai|gemini|deepseek|groq)$")
    llm_model: str = ""
    stt_model: str = ""
    tts_provider: str = Field(default="", pattern="^(|openrouter|edge)$")
    tts_model: str = ""
    tts_voice: str = ""
    history_turns: int = Field(default=6, ge=0, le=30)


def _agent_cols(body: AgentIn) -> dict:
    cols = body.model_dump()
    # Model rieng ma khong chon nha cung cap -> OpenRouter; "mac dinh he thong" thi khong co model.
    if cols["llm_model"].strip() and not cols["llm_provider"]:
        cols["llm_provider"] = "openrouter"
    cols["llm_model"] = cols["llm_model"].strip()
    return cols


def _own_agent(aid: int, user: dict) -> dict:
    agent = db.row("SELECT * FROM agents WHERE id = ?", aid)
    if not agent or (agent["user_id"] != user["id"] and user["role"] != "admin"):
        raise HTTPException(404, "agent_not_found")
    return agent


@router.get("/agents")
def list_agents(user=Depends(current_user)):
    return db.rows(
        "SELECT a.*, (SELECT COUNT(*) FROM devices d WHERE d.agent_id = a.id) AS device_count "
        "FROM agents a WHERE a.user_id = ? ORDER BY a.id", user["id"])


@router.post("/agents")
def create_agent(body: AgentIn, user=Depends(current_user)):
    cols = _agent_cols(body)
    aid = db.execute(
        f"INSERT INTO agents(user_id, created_at, {', '.join(cols)}) VALUES(?, ?, {', '.join('?' * len(cols))})",
        user["id"], time.time(), *cols.values())
    return db.row("SELECT * FROM agents WHERE id = ?", aid)


@router.get("/agents/{aid}")
def get_agent(aid: int, user=Depends(current_user)):
    return _own_agent(aid, user)


@router.put("/agents/{aid}")
def update_agent(aid: int, body: AgentIn, user=Depends(current_user)):
    _own_agent(aid, user)
    cols = _agent_cols(body)
    db.execute(f"UPDATE agents SET {', '.join(f'{k} = ?' for k in cols)} WHERE id = ?", *cols.values(), aid)
    return db.row("SELECT * FROM agents WHERE id = ?", aid)


@router.delete("/agents/{aid}")
def delete_agent(aid: int, user=Depends(current_user)):
    _own_agent(aid, user)
    db.execute("DELETE FROM agents WHERE id = ?", aid)
    return {"ok": True}


# ---------- thiet bi ----------

def _device_view(d: dict) -> dict:
    s = ONLINE.get(d["mac"])
    return {**d, "online": s is not None, "state": s.state if s else None}


@router.get("/devices")
def list_devices(user=Depends(current_user)):
    rows = db.rows(
        "SELECT d.*, a.name AS agent_name FROM devices d LEFT JOIN agents a ON a.id = d.agent_id "
        "WHERE d.user_id = ? ORDER BY d.id", user["id"])
    return [_device_view(d) for d in rows]


class BindIn(BaseModel):
    code: str = Field(pattern=r"^\s*\d{6}\s*$")
    agent_id: int | None = None
    name: str = ""


@router.post("/devices/bind")
def bind(body: BindIn, user=Depends(current_user)):
    if body.agent_id is not None:
        _own_agent(body.agent_id, user)
    dev = bind_device(body.code, user["id"], body.agent_id)
    if not dev:
        raise HTTPException(404, "invalid_code")
    if body.name:
        db.execute("UPDATE devices SET name = ? WHERE id = ?", body.name.strip(), dev["id"])
    return {"ok": True, "id": dev["id"]}


def _own_device(did: int, user: dict) -> dict:
    dev = db.row("SELECT * FROM devices WHERE id = ?", did)
    if not dev or dev["user_id"] != user["id"]:
        raise HTTPException(404, "device_not_found")
    return dev


class DeviceUpdate(BaseModel):
    name: str | None = Field(default=None, max_length=60)
    agent_id: int | None = None


@router.put("/devices/{did}")
async def update_device(did: int, body: DeviceUpdate, user=Depends(current_user)):
    dev = _own_device(did, user)
    if body.agent_id is not None:
        _own_agent(body.agent_id, user)
        db.execute("UPDATE devices SET agent_id = ? WHERE id = ?", body.agent_id, did)
    if body.name is not None:
        db.execute("UPDATE devices SET name = ? WHERE id = ?", body.name.strip(), did)
    # Doi tac nhan: ap dung cho phien dang mo luon.
    session = ONLINE.get(dev["mac"])
    if session and body.agent_id is not None:
        session.agent = db.row("SELECT * FROM agents WHERE id = ?", body.agent_id)
        session.history.clear()
    return {"ok": True}


@router.delete("/devices/{did}")
async def unbind(did: int, user=Depends(current_user)):
    dev = _own_device(did, user)
    db.execute("UPDATE devices SET user_id = NULL, agent_id = NULL WHERE id = ?", did)
    session = ONLINE.get(dev["mac"])
    if session:
        await session.ws.close(code=4403, reason="unbound")
    return {"ok": True}


@router.get("/devices/{did}/messages")
def device_messages(did: int, limit: int = 100, user=Depends(current_user)):
    _own_device(did, user)
    msgs = db.rows("SELECT role, content, created_at FROM messages WHERE device_id = ? ORDER BY id DESC LIMIT ?",
                   did, min(limit, 500))
    return list(reversed(msgs))


@router.delete("/devices/{did}/messages")
def clear_messages(did: int, user=Depends(current_user)):
    _own_device(did, user)
    db.execute("DELETE FROM messages WHERE device_id = ?", did)
    return {"ok": True}
