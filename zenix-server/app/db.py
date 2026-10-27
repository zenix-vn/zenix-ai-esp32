"""SQLite: mot file, du cho quy mo nho. Moi ham mo ket noi rieng (an toan voi thread)."""
import sqlite3
import time
from contextlib import contextmanager

from .config import DB_PATH

SCHEMA = """
CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY,
    username TEXT UNIQUE NOT NULL,
    password_hash TEXT NOT NULL,
    role TEXT NOT NULL DEFAULT 'user',
    created_at REAL NOT NULL
);
CREATE TABLE IF NOT EXISTS settings (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL
);
CREATE TABLE IF NOT EXISTS agents (
    id INTEGER PRIMARY KEY,
    user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    name TEXT NOT NULL,
    language TEXT NOT NULL DEFAULT 'vi',
    system_prompt TEXT NOT NULL DEFAULT '',
    llm_model TEXT NOT NULL DEFAULT '',
    stt_model TEXT NOT NULL DEFAULT '',
    tts_provider TEXT NOT NULL DEFAULT '',
    tts_model TEXT NOT NULL DEFAULT '',
    tts_voice TEXT NOT NULL DEFAULT '',
    history_turns INTEGER NOT NULL DEFAULT 6,
    created_at REAL NOT NULL
);
CREATE TABLE IF NOT EXISTS devices (
    id INTEGER PRIMARY KEY,
    mac TEXT UNIQUE NOT NULL,
    client_id TEXT NOT NULL DEFAULT '',
    user_id INTEGER REFERENCES users(id) ON DELETE SET NULL,
    agent_id INTEGER REFERENCES agents(id) ON DELETE SET NULL,
    name TEXT NOT NULL DEFAULT '',
    board TEXT NOT NULL DEFAULT '',
    firmware TEXT NOT NULL DEFAULT '',
    last_ip TEXT NOT NULL DEFAULT '',
    last_seen REAL,
    created_at REAL NOT NULL
);
CREATE TABLE IF NOT EXISTS activation_codes (
    code TEXT PRIMARY KEY,
    mac TEXT UNIQUE NOT NULL,
    expires_at REAL NOT NULL
);
CREATE TABLE IF NOT EXISTS messages (
    id INTEGER PRIMARY KEY,
    device_id INTEGER NOT NULL REFERENCES devices(id) ON DELETE CASCADE,
    role TEXT NOT NULL,
    content TEXT NOT NULL,
    created_at REAL NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_messages_device ON messages(device_id, id);
CREATE TABLE IF NOT EXISTS plans (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    description TEXT NOT NULL DEFAULT '',
    price_label TEXT NOT NULL DEFAULT '',
    llm_provider TEXT NOT NULL DEFAULT 'openrouter',
    llm_model TEXT NOT NULL DEFAULT '',
    api_key TEXT NOT NULL DEFAULT '',
    stt_model TEXT NOT NULL DEFAULT '',
    tts_provider TEXT NOT NULL DEFAULT '',
    tts_model TEXT NOT NULL DEFAULT '',
    tts_voice TEXT NOT NULL DEFAULT '',
    created_at REAL NOT NULL
);
CREATE TABLE IF NOT EXISTS user_keys (
    user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    provider TEXT NOT NULL,
    api_key TEXT NOT NULL,
    created_at REAL NOT NULL,
    PRIMARY KEY (user_id, provider)
);
"""

# Cot them sau khi da trien khai: (bang, cot, dinh nghia).
MIGRATIONS = [
    ("agents", "llm_provider", "TEXT NOT NULL DEFAULT ''"),
    ("users", "plan_id", "INTEGER REFERENCES plans(id) ON DELETE SET NULL"),
    ("users", "plan_expires_at", "REAL"),
]


def connect() -> sqlite3.Connection:
    conn = sqlite3.connect(DB_PATH, timeout=10)
    conn.row_factory = sqlite3.Row
    conn.execute("PRAGMA foreign_keys = ON")
    return conn


@contextmanager
def tx():
    conn = connect()
    try:
        yield conn
        conn.commit()
    finally:
        conn.close()


def init() -> None:
    with tx() as c:
        c.execute("PRAGMA journal_mode = WAL")
        c.executescript(SCHEMA)
        for table, column, ddl in MIGRATIONS:
            cols = {r["name"] for r in c.execute(f"PRAGMA table_info({table})")}
            if column not in cols:
                c.execute(f"ALTER TABLE {table} ADD COLUMN {column} {ddl}")
        # Tac nhan cu: co model ma chua co nha cung cap -> OpenRouter.
        c.execute("UPDATE agents SET llm_provider = 'openrouter' WHERE llm_provider = '' AND llm_model != ''")


def row(sql: str, *args):
    with tx() as c:
        r = c.execute(sql, args).fetchone()
        return dict(r) if r else None


def rows(sql: str, *args) -> list[dict]:
    with tx() as c:
        return [dict(r) for r in c.execute(sql, args).fetchall()]


def execute(sql: str, *args) -> int:
    """Tra ve lastrowid."""
    with tx() as c:
        return c.execute(sql, args).lastrowid


def now() -> float:
    return time.time()


def get_setting(key: str, default: str = "") -> str:
    r = row("SELECT value FROM settings WHERE key = ?", key)
    return r["value"] if r else default


def set_setting(key: str, value: str) -> None:
    execute(
        "INSERT INTO settings(key, value) VALUES(?, ?) ON CONFLICT(key) DO UPDATE SET value = excluded.value",
        key,
        value,
    )
