"""Nha cung cap LLM tuong thich OpenAI Chat Completions: stream + tool calling, danh sach model, gia."""
import json
import re
import time
from collections.abc import AsyncIterator

import httpx

from .config import OPENROUTER_BASE
from .providers import openrouter
from .providers.openrouter import ProviderError

# or_prefix: tien to tren OpenRouter de tra gia cho model goi truc tiep.
PROVIDERS = {
    "openrouter": {"name": "OpenRouter", "base": OPENROUTER_BASE, "keys_url": "https://openrouter.ai/keys"},
    "openai": {"name": "OpenAI (ChatGPT)", "base": "https://api.openai.com/v1",
               "keys_url": "https://platform.openai.com/api-keys", "or_prefix": "openai/"},
    "gemini": {"name": "Google Gemini", "base": "https://generativelanguage.googleapis.com/v1beta/openai",
               "keys_url": "https://aistudio.google.com/apikey", "or_prefix": "google/"},
    "deepseek": {"name": "DeepSeek", "base": "https://api.deepseek.com/v1",
                 "keys_url": "https://platform.deepseek.com/api_keys", "or_prefix": "deepseek/"},
    "groq": {"name": "Groq", "base": "https://api.groq.com/openai/v1", "keys_url": "https://console.groq.com/keys"},
}

# Bo cac model khong dung de tro chuyen khi liet ke tu nha cung cap truc tiep.
_NON_CHAT = re.compile(r"(embed|whisper|tts|audio|transcri|dall-e|image|moderation|realtime|search|davinci|babbage|aqa|imagen|veo|guard|computer-use)", re.I)


def _headers(provider: str, key: str) -> dict:
    h = {"Authorization": f"Bearer {key}"}
    if provider == "openrouter":
        h.update({"HTTP-Referer": "https://dev.zenix.vn", "X-Title": "Zenix AI"})
    return h


async def _raise_for(resp: httpx.Response, provider: str) -> None:
    if resp.status_code >= 400:
        body = (await resp.aread()).decode(errors="replace")[:300]
        raise ProviderError(f"{provider}_http_{resp.status_code}: {body}")


async def chat_stream(provider: str, key: str, model: str, messages: list[dict],
                      tools: list[dict] | None = None) -> AsyncIterator[dict]:
    """Yield {"text": delta} cho tung manh van ban, va mot lan {"tool_calls": [...]} neu model goi tool."""
    base = PROVIDERS[provider]["base"]
    payload: dict = {"model": model, "messages": messages, "stream": True}
    if tools:
        payload["tools"] = tools
    calls: dict[int, dict] = {}
    async with httpx.AsyncClient(timeout=httpx.Timeout(60, connect=10)) as client:
        async with client.stream("POST", f"{base}/chat/completions", headers=_headers(provider, key), json=payload) as resp:
            await _raise_for(resp, provider)
            async for line in resp.aiter_lines():
                if not line.startswith("data:"):
                    continue  # dong keep-alive
                data = line[5:].strip()
                if data == "[DONE]":
                    break
                chunk = json.loads(data)
                if "error" in chunk:
                    raise ProviderError(str(chunk["error"])[:300])
                choices = chunk.get("choices") or []
                if not choices:
                    continue
                delta = choices[0].get("delta") or {}
                if delta.get("content"):
                    yield {"text": delta["content"]}
                for tc in delta.get("tool_calls") or []:
                    slot = calls.setdefault(tc.get("index", 0), {"id": "", "name": "", "arguments": ""})
                    slot["id"] = tc.get("id") or slot["id"] or f"call_{tc.get('index', 0)}"
                    fn = tc.get("function") or {}
                    slot["name"] += fn.get("name") or ""
                    slot["arguments"] += fn.get("arguments") or ""
    if calls:
        yield {"tool_calls": [calls[i] for i in sorted(calls)]}


async def verify_key(provider: str, key: str) -> None:
    if provider == "openrouter":
        await openrouter.check_key(key)
        return
    async with httpx.AsyncClient(timeout=15) as client:
        resp = await client.get(f"{PROVIDERS[provider]['base']}/models", headers=_headers(provider, key))
    if resp.status_code != 200:
        raise ProviderError(f"invalid_key ({resp.status_code})")


def _price_index(catalog: list[dict]) -> dict[str, dict]:
    return {m["id"]: m for m in catalog}


def _lookup_price(index: dict[str, dict], prefix: str, model_id: str) -> dict | None:
    base = re.sub(r"(-\d{4}-\d{2}-\d{2}|-\d{8}|-\d{4}|-latest|-preview.*)$", "", model_id)
    for cand in (prefix + model_id, prefix + base):
        if cand in index:
            return index[cand]
    return None


_direct_cache: dict[tuple[str, str], tuple[float, list[dict]]] = {}


async def list_llm_models(provider: str, key: str | None) -> list[dict]:
    """Model chat cua nha cung cap kem gia (USD / 1 trieu token) neu biet."""
    catalog = await openrouter.list_models("llm")
    if provider == "openrouter":
        return catalog
    if not key:
        raise ProviderError("missing_api_key")
    cache_key = (provider, key[-8:])
    cached = _direct_cache.get(cache_key)
    if cached and time.time() - cached[0] < 600:
        return cached[1]
    async with httpx.AsyncClient(timeout=20) as client:
        resp = await client.get(f"{PROVIDERS[provider]['base']}/models", headers=_headers(provider, key))
    if resp.status_code != 200:
        raise ProviderError(f"{provider}_http_{resp.status_code}")
    index = _price_index(catalog)
    prefix = PROVIDERS[provider].get("or_prefix", "")
    out = []
    for m in resp.json().get("data", []):
        mid = m["id"].removeprefix("models/")
        if _NON_CHAT.search(mid):
            continue
        item = {"id": mid, "name": m.get("display_name") or mid, "price_in": None, "price_out": None,
                "context": None, "tools": None, "approx": True}
        ref = _lookup_price(index, prefix, mid) if prefix else None
        if ref:
            item.update({k: ref[k] for k in ("price_in", "price_out", "context", "tools")})
        out.append(item)
    out.sort(key=lambda x: x["id"])
    _direct_cache[cache_key] = (time.time(), out)
    return out
