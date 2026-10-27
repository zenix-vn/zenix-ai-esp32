"""OpenRouter: speech-to-text, text-to-speech, danh muc model (kem gia), kiem tra key.

Chat completions (dung chung cho moi nha cung cap) nam o app/llm.py.
"""
import base64
import time

import httpx

from ..config import OPENROUTER_BASE


class ProviderError(Exception):
    pass


def _headers(key: str) -> dict:
    return {"Authorization": f"Bearer {key}", "HTTP-Referer": "https://dev.zenix.vn", "X-Title": "Zenix AI"}


async def _raise_for(resp: httpx.Response) -> None:
    if resp.status_code >= 400:
        body = (await resp.aread()).decode(errors="replace")[:300]
        raise ProviderError(f"openrouter_http_{resp.status_code}: {body}")


async def transcribe(key: str, model: str, wav: bytes, language: str | None) -> str:
    payload = {
        "model": model,
        "input_audio": {"data": base64.b64encode(wav).decode(), "format": "wav"},
    }
    if language:
        payload["language"] = language
    async with httpx.AsyncClient(timeout=60) as client:
        resp = await client.post(f"{OPENROUTER_BASE}/audio/transcriptions", headers=_headers(key), json=payload)
        await _raise_for(resp)
        return (resp.json().get("text") or "").strip()


async def speech_pcm(key: str, model: str, voice: str, text: str, instructions: str = "") -> bytes:
    """Tra ve PCM 16-bit mono 24 kHz."""
    payload = {"model": model, "input": text, "voice": voice, "response_format": "pcm"}
    if instructions:
        payload["instructions"] = instructions
    async with httpx.AsyncClient(timeout=60) as client:
        resp = await client.post(f"{OPENROUTER_BASE}/audio/speech", headers=_headers(key), json=payload)
        await _raise_for(resp)
        return resp.content


def _per_million(value) -> float | None:
    try:
        return round(float(value) * 1_000_000, 4)
    except (TypeError, ValueError):
        return None


def _audio_price(kind: str, m: dict) -> dict:
    """Gia STT/TTS. OpenRouter ghi gia theo don vi cua tung model:
    - completion = 0: STT tinh theo giay am thanh (hien $/phut), TTS tinh theo ky tu (hien $/1M ky tu);
    - con lai: theo token nhu LLM ($/1M token vao/ra).
    """
    pricing = m.get("pricing") or {}
    try:
        p_in, p_out = float(pricing.get("prompt") or 0), float(pricing.get("completion") or 0)
    except ValueError:
        return {"price_in": None, "price_out": None, "unit": "token"}
    if p_in < 0 or p_out < 0 or (p_in == 0 and p_out == 0 and not m["id"].endswith(":free")):
        return {"price_in": None, "price_out": None, "unit": "token"}
    if p_out == 0:
        if kind == "stt":
            return {"price_in": round(p_in * 60, 5), "price_out": 0.0, "unit": "minute"}
        return {"price_in": round(p_in * 1_000_000, 4), "price_out": 0.0, "unit": "mchar"}
    return {"price_in": round(p_in * 1_000_000, 4), "price_out": round(p_out * 1_000_000, 4), "unit": "token"}


_models_cache: dict[str, tuple[float, list[dict]]] = {}


async def list_models(kind: str) -> list[dict]:
    """kind: llm | stt | tts. Gia tinh theo USD / 1 trieu token (LLM). Cache 10 phut."""
    cached = _models_cache.get(kind)
    if cached and time.time() - cached[0] < 600:
        return cached[1]
    params = {"stt": {"output_modalities": "transcription"}, "tts": {"output_modalities": "speech"}}.get(kind, {})
    async with httpx.AsyncClient(timeout=20) as client:
        resp = await client.get(f"{OPENROUTER_BASE}/models", params=params)
        resp.raise_for_status()
        data = resp.json().get("data", [])
    out = []
    for m in data:
        item = {"id": m["id"], "name": m.get("name", m["id"])}
        if kind == "llm":
            # Tro chuyen bang giong: chi can model tra ve van ban (bo model tao anh/nhac/am thanh).
            arch = m.get("architecture") or {}
            if (arch.get("output_modalities") or ["text"]) != ["text"]:
                continue
            pricing = m.get("pricing") or {}
            item.update({
                "price_in": _per_million(pricing.get("prompt")),
                "price_out": _per_million(pricing.get("completion")),
                "context": m.get("context_length"),
                "tools": "tools" in (m.get("supported_parameters") or []),
                "approx": False,
                "unit": "token",
            })
            # Gia am (-1) = gia dong theo router; gia 0 ma khong phai ban ":free" = tinh phi kieu khac.
            # Ca hai truong hop: coi nhu chua biet gia.
            zero = item["price_in"] == 0 and item["price_out"] == 0
            if (item["price_in"] or 0) < 0 or (item["price_out"] or 0) < 0 or (zero and not m["id"].endswith(":free")):
                item["price_in"] = item["price_out"] = None
        else:
            item.update(_audio_price(kind, m))
        if kind == "tts":
            item["voices"] = m.get("supported_voices") or []
        out.append(item)
    out.sort(key=lambda x: x["id"])
    _models_cache[kind] = (time.time(), out)
    return out


async def check_key(key: str) -> dict:
    async with httpx.AsyncClient(timeout=15) as client:
        resp = await client.get(f"{OPENROUTER_BASE}/key", headers={"Authorization": f"Bearer {key}"})
    if resp.status_code != 200:
        raise ProviderError(f"invalid_key ({resp.status_code})")
    return resp.json().get("data", {})
