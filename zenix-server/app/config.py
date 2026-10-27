"""Cau hinh tu bien moi truong."""
import os
import secrets
from pathlib import Path

DATA_DIR = Path(os.environ.get("ZENIX_DATA_DIR", "./data")).resolve()
DATA_DIR.mkdir(parents=True, exist_ok=True)
DB_PATH = DATA_DIR / "zenix.db"

# URL cong khai, dung de tra ve cho thiet bi (vd. https://dev.zenix.vn).
PUBLIC_URL = os.environ.get("ZENIX_PUBLIC_URL", "http://localhost:8080").rstrip("/")


def _load_secret() -> str:
    env = os.environ.get("ZENIX_SECRET_KEY")
    if env:
        return env
    path = DATA_DIR / "secret.key"
    if not path.exists():
        path.write_text(secrets.token_hex(32))
        path.chmod(0o600)
    return path.read_text().strip()


SECRET_KEY = _load_secret()

OPENROUTER_BASE = os.environ.get("ZENIX_OPENROUTER_BASE", "https://openrouter.ai/api/v1")

# Am thanh: thiet bi gui 16 kHz, server gui 24 kHz; khung Opus 60 ms.
UPLINK_RATE = 16000
DOWNLINK_RATE = 24000
FRAME_MS = 60

DEFAULTS = {
    "llm_model": "google/gemini-2.5-flash",
    "stt_model": "openai/whisper-large-v3",
    "tts_provider": "edge",
    "tts_model": "google/gemini-3.8-flash-tts",
    "tts_voice": "",  # trong = chon theo ngon ngu (xem session.pick_voice)
}


def ws_url() -> str:
    base = PUBLIC_URL.replace("https://", "wss://").replace("http://", "ws://")
    return f"{base}/xiaozhi/v1/"
