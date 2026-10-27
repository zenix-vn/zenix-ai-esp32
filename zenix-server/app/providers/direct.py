"""Giong noi goi thang nha cung cap (khi nguoi dung chi co key OpenAI/Groq rieng)."""
import httpx

from .openrouter import ProviderError

BASES = {"openai": "https://api.openai.com/v1", "groq": "https://api.groq.com/openai/v1"}


async def transcribe(provider: str, key: str, model: str, wav: bytes, language: str | None) -> str:
    data = {"model": model}
    if language:
        data["language"] = language
    async with httpx.AsyncClient(timeout=60) as client:
        resp = await client.post(f"{BASES[provider]}/audio/transcriptions", headers={"Authorization": f"Bearer {key}"},
                                 data=data, files={"file": ("speech.wav", wav, "audio/wav")})
    if resp.status_code >= 400:
        raise ProviderError(f"{provider}_http_{resp.status_code}: {resp.text[:300]}")
    return (resp.json().get("text") or "").strip()


async def speech_pcm(provider: str, key: str, model: str, voice: str, text: str) -> bytes:
    """OpenAI TTS: PCM 16-bit mono 24 kHz."""
    async with httpx.AsyncClient(timeout=60) as client:
        resp = await client.post(f"{BASES[provider]}/audio/speech", headers={"Authorization": f"Bearer {key}"},
                                 json={"model": model, "input": text, "voice": voice, "response_format": "pcm"})
    if resp.status_code >= 400:
        raise ProviderError(f"{provider}_http_{resp.status_code}: {resp.text[:300]}")
    return resp.content
