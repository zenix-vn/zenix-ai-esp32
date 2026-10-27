"""Mot phien WebSocket voi thiet bi: nghe -> STT -> LLM (+ tool MCP) -> TTS -> Opus.

Giao thuc (tuong thich firmware Zin):
  thiet bi -> server: hello, listen{start|stop|detect}, abort, mcp (tra loi), khung Opus 16 kHz
  server -> thiet bi: hello, stt, llm, tts{start|sentence_start|stop}, mcp (yeu cau), alert, khung Opus 24 kHz
"""
import asyncio
import json
import logging
import time
import uuid

from fastapi import WebSocket, WebSocketDisconnect

from . import db, keys, llm, runtime
from .audio import DownlinkEncoder, UplinkDecoder, pcm_to_wav
from .config import DEFAULTS, DOWNLINK_RATE, FRAME_MS, UPLINK_RATE
from .providers import direct, edge, openrouter
from .text import SentenceSplitter, clean_for_speech

log = logging.getLogger("zenix.session")

# Thiet bi dang online: mac -> Session (trang quan tri dung de hien trang thai).
ONLINE: dict[str, "Session"] = {}

FRAME_S = FRAME_MS / 1000
PREBUFFER_S = 0.36  # gui truoc ~6 khung de thiet bi khong bi hut am
MAX_TOOL_ROUNDS = 3
MIN_SPEECH_S = 0.3

LANG_RULES = {
    "vi": "Luôn trả lời bằng tiếng Việt tự nhiên.",
    "en": "Always reply in natural English.",
}
VOICE_RULES = (
    "Your reply is spoken aloud by a small voice device. Keep it short and conversational "
    "(usually 1-3 sentences), no markdown, no lists, no emoji, write numbers the way they are spoken. "
    "Use the available tools when the user asks to control or inspect the device."
)


def default_agent() -> dict:
    return {
        "id": None, "user_id": None, "name": "Zin", "language": "vi", "history_turns": 6, "llm_provider": "",
        "system_prompt": "Bạn là Zin, trợ lý giọng nói thân thiện.",
        **{k: "" for k in ("llm_model", "stt_model", "tts_provider", "tts_model", "tts_voice")},
    }


def access_error(e: Exception) -> str:
    """Thong bao gui ve thiet bi khi khong duoc phep dung AI."""
    if isinstance(e, keys.NoPlan):
        return "no_plan: tai khoan chua co goi cuoc con han; mua goi hoac them API key rieng"
    return f"missing_api_key:{e}"


def pick_voice(provider: str, language: str, voice: str) -> str:
    """Giong da chon; neu trong hoac khong hop voi nha cung cap thi chon theo ngon ngu."""
    if provider == "edge":
        voices = edge.VOICES.get(language, edge.VOICES["en"])
        return voice if voice in sum(edge.VOICES.values(), []) else voices[0]
    return voice or ("alloy" if provider == "openai" else "Kore")


class Session:
    def __init__(self, ws: WebSocket, device: dict, agent: dict):
        self.ws = ws
        self.device = device
        self.agent = agent
        self.session_id = uuid.uuid4().hex
        self.state = "idle"  # idle | listening | thinking | speaking
        self.connected_at = time.time()

        self._decoder = UplinkDecoder()
        self._encoder = DownlinkEncoder()
        self._pcm = bytearray()
        self._send_lock = asyncio.Lock()
        self._task: asyncio.Task | None = None
        self._play_clock = 0.0
        self._rt: runtime.Runtime | None = None

        self.history: list[dict] = []
        self.tools: list[dict] = []          # dinh dang OpenAI function
        self._tool_names: dict[str, str] = {}  # ten an toan -> ten MCP goc
        self._mcp_id = 0
        self._mcp_pending: dict[int, asyncio.Future] = {}

    # ---------- gui ----------

    async def send_json(self, msg: dict) -> None:
        msg.setdefault("session_id", self.session_id)
        async with self._send_lock:
            await self.ws.send_text(json.dumps(msg, ensure_ascii=False))

    async def send_audio_paced(self, frames: list[bytes]) -> None:
        """Gui khung Opus theo nhip thoi gian thuc de lenh abort co hieu luc ngay."""
        loop = asyncio.get_running_loop()
        for frame in frames:
            now = loop.time()
            self._play_clock = max(self._play_clock, now)
            wait = self._play_clock - now - PREBUFFER_S
            if wait > 0:
                await asyncio.sleep(wait)
            async with self._send_lock:
                await self.ws.send_bytes(frame)
            self._play_clock += FRAME_S

    async def alert(self, message: str) -> None:
        log.warning("[%s] %s", self.device["mac"], message)
        await self.send_json({"type": "alert", "status": "error", "message": message, "emotion": "sad"})

    # ---------- vong doi ----------

    async def run(self) -> None:
        ONLINE[self.device["mac"]] = self
        try:
            while True:
                msg = await self.ws.receive()
                if msg["type"] == "websocket.disconnect":
                    break
                if msg.get("bytes") is not None:
                    self.on_audio(msg["bytes"])
                elif msg.get("text") is not None:
                    await self.on_text(msg["text"])
        except WebSocketDisconnect:
            pass
        finally:
            if ONLINE.get(self.device["mac"]) is self:
                ONLINE.pop(self.device["mac"], None)
            await self.cancel_response()
            for fut in self._mcp_pending.values():
                fut.cancel()
            db.execute("UPDATE devices SET last_seen = ? WHERE id = ?", time.time(), self.device["id"])

    def on_audio(self, packet: bytes) -> None:
        if self.state != "listening":
            return
        try:
            self._pcm += self._decoder.decode(packet)
        except Exception as e:  # khung hong: bo qua
            log.debug("opus decode failed: %s", e)

    async def on_text(self, text: str) -> None:
        try:
            msg = json.loads(text)
        except ValueError:
            return
        kind = msg.get("type")
        if kind == "hello":
            await self.on_hello(msg)
        elif kind == "listen":
            await self.on_listen(msg)
        elif kind == "abort":
            await self.cancel_response()
            await self.send_json({"type": "tts", "state": "stop"})
        elif kind == "mcp":
            self.on_mcp_message(msg.get("payload") or {})

    async def on_hello(self, msg: dict) -> None:
        await self.send_json({
            "type": "hello",
            "transport": "websocket",
            "audio_params": {"format": "opus", "sample_rate": DOWNLINK_RATE, "channels": 1, "frame_duration": FRAME_MS},
        })
        if (msg.get("features") or {}).get("mcp"):
            asyncio.create_task(self.discover_tools())

    async def on_listen(self, msg: dict) -> None:
        state = msg.get("state")
        if state == "start":
            await self.cancel_response()
            self._pcm.clear()
            self.state = "listening"
        elif state == "stop" and self.state == "listening":
            self.state = "thinking"
            pcm, self._pcm = bytes(self._pcm), bytearray()
            self._task = asyncio.create_task(self.respond_to_audio(pcm))
        elif state == "detect" and msg.get("text"):
            # Wake word: coi nhu nguoi dung goi ten.
            await self.cancel_response()
            self._task = asyncio.create_task(self.respond_to_text(msg["text"]))

    async def cancel_response(self) -> None:
        task, self._task = self._task, None
        if task and not task.done():
            task.cancel()
            try:
                await task
            except (asyncio.CancelledError, Exception):
                pass
        self._play_clock = 0.0
        if self.state in ("thinking", "speaking"):
            self.state = "idle"

    # ---------- xu ly mot luot hoi thoai ----------

    async def respond_to_audio(self, pcm: bytes) -> None:
        if len(pcm) < UPLINK_RATE * 2 * MIN_SPEECH_S:
            self.state = "idle"
            return
        lang = self.agent.get("language") or "vi"
        try:
            self._rt = rt = runtime.resolve(self.agent)
            if rt.stt_backend == "openrouter":
                text = await openrouter.transcribe(rt.stt_key, rt.stt_model, pcm_to_wav(pcm), lang)
            else:
                text = await direct.transcribe(rt.stt_backend, rt.stt_key, rt.stt_model, pcm_to_wav(pcm), lang)
        except (keys.KeyMissing, keys.NoPlan) as e:
            self.state = "idle"
            await self.alert(access_error(e))
            return
        except openrouter.ProviderError as e:
            self.state = "idle"
            await self.alert(f"STT: {e}")
            return
        await self.send_json({"type": "stt", "text": text})
        if not text:
            self.state = "idle"
            return
        await self.respond_to_text(text)

    async def respond_to_text(self, user_text: str) -> None:
        self.state = "thinking"
        self.save_message("user", user_text)
        messages = self.build_messages(user_text)
        reply = ""
        tts_started = False
        speak_queue: asyncio.Queue = asyncio.Queue()
        speaker = asyncio.create_task(self.speak_worker(speak_queue))
        try:
            if self._rt is None:  # luot bat dau tu wake word (chua qua STT)
                self._rt = runtime.resolve(self.agent)
            rt = self._rt
            for _ in range(MAX_TOOL_ROUNDS + 1):
                splitter = SentenceSplitter()
                tool_calls = None
                async for chunk in llm.chat_stream(rt.llm_provider, rt.llm_key, rt.llm_model, messages, self.tools or None):
                    if "text" in chunk:
                        reply += chunk["text"]
                        for sentence in splitter.feed(chunk["text"]):
                            if not tts_started:
                                tts_started = True
                                await self.send_json({"type": "tts", "state": "start"})
                            speak_queue.put_nowait(self.start_tts(sentence))
                    else:
                        tool_calls = chunk["tool_calls"]
                for sentence in splitter.flush():
                    if not tts_started:
                        tts_started = True
                        await self.send_json({"type": "tts", "state": "start"})
                    speak_queue.put_nowait(self.start_tts(sentence))
                if not tool_calls:
                    break
                messages.append({"role": "assistant", "content": None, "tool_calls": [
                    {"id": c["id"], "type": "function", "function": {"name": c["name"], "arguments": c["arguments"] or "{}"}}
                    for c in tool_calls
                ]})
                for call in tool_calls:
                    result = await self.call_device_tool(call["name"], call["arguments"])
                    messages.append({"role": "tool", "tool_call_id": call["id"], "content": result})
            speak_queue.put_nowait(None)
            await speaker
        except (keys.KeyMissing, keys.NoPlan) as e:
            speaker.cancel()
            await self.alert(access_error(e))
        except openrouter.ProviderError as e:
            speaker.cancel()
            await self.alert(f"LLM: {e}")
        except asyncio.CancelledError:
            speaker.cancel()
            raise
        finally:
            if reply.strip():
                self.save_message("assistant", reply.strip())
                self.history += [{"role": "user", "content": user_text}, {"role": "assistant", "content": reply.strip()}]
                keep = max(0, int(self.agent.get("history_turns") or 6)) * 2
                self.history = self.history[-keep:] if keep else []
            if tts_started:
                try:
                    await self.send_json({"type": "tts", "state": "stop"})
                except Exception:
                    pass
            self._rt = None
            self.state = "idle"

    def start_tts(self, sentence: str) -> tuple[str, asyncio.Task]:
        """Bat dau tong hop ngay (song song voi cau dang phat) de giam do tre."""
        return sentence, asyncio.create_task(self.synthesize(clean_for_speech(sentence)))

    async def speak_worker(self, queue: asyncio.Queue) -> None:
        while True:
            item = await queue.get()
            if item is None:
                return
            sentence, task = item
            try:
                pcm = await task
            except Exception as e:  # loi TTS mot cau: bao va doc tiep cau sau
                await self.alert(f"TTS: {e}")
                continue
            self.state = "speaking"
            await self.send_json({"type": "tts", "state": "sentence_start", "text": sentence})
            await self.send_audio_paced(self._encoder.encode(pcm))

    async def synthesize(self, text: str) -> bytes:
        rt = self._rt
        voice = pick_voice(rt.tts_provider, self.agent.get("language") or "vi", rt.tts_voice)
        if rt.tts_provider == "edge":
            return await edge.speech_pcm(voice, text)
        if rt.tts_provider == "openai":
            return await direct.speech_pcm("openai", rt.tts_key, rt.tts_model, voice, text)
        return await openrouter.speech_pcm(rt.tts_key, rt.tts_model, voice, text)

    def build_messages(self, user_text: str) -> list[dict]:
        lang = self.agent.get("language") or "vi"
        system = "\n\n".join(filter(None, [
            (self.agent.get("system_prompt") or "").strip(),
            LANG_RULES.get(lang, ""),
            VOICE_RULES,
            f"Current time: {time.strftime('%Y-%m-%d %H:%M', time.localtime())}.",
        ]))
        return [{"role": "system", "content": system}, *self.history, {"role": "user", "content": user_text}]

    def save_message(self, role: str, content: str) -> None:
        db.execute(
            "INSERT INTO messages(device_id, role, content, created_at) VALUES(?, ?, ?, ?)",
            self.device["id"], role, content, time.time(),
        )

    # ---------- MCP: tool tren thiet bi ----------

    async def mcp_request(self, method: str, params: dict | None = None, timeout: float = 8) -> dict:
        self._mcp_id += 1
        req_id = self._mcp_id
        fut = asyncio.get_running_loop().create_future()
        self._mcp_pending[req_id] = fut
        payload = {"jsonrpc": "2.0", "id": req_id, "method": method}
        if params is not None:
            payload["params"] = params
        await self.send_json({"type": "mcp", "payload": payload})
        try:
            return await asyncio.wait_for(fut, timeout)
        finally:
            self._mcp_pending.pop(req_id, None)

    def on_mcp_message(self, payload: dict) -> None:
        fut = self._mcp_pending.get(payload.get("id"))
        if fut and not fut.done():
            fut.set_result(payload)

    async def discover_tools(self) -> None:
        try:
            await self.mcp_request("initialize", {"protocolVersion": "2024-11-05", "capabilities": {}})
            tools, cursor = [], ""
            for _ in range(10):
                resp = await self.mcp_request("tools/list", {"cursor": cursor})
                result = resp.get("result") or {}
                tools += result.get("tools") or []
                cursor = result.get("nextCursor") or ""
                if not cursor:
                    break
        except (asyncio.TimeoutError, asyncio.CancelledError):
            log.info("[%s] device did not answer MCP", self.device["mac"])
            return
        self.tools, self._tool_names = [], {}
        for t in tools:
            safe = "".join(ch if ch.isalnum() or ch in "_-" else "_" for ch in t["name"])[:64]
            self._tool_names[safe] = t["name"]
            self.tools.append({"type": "function", "function": {
                "name": safe,
                "description": t.get("description", ""),
                "parameters": t.get("inputSchema") or {"type": "object", "properties": {}},
            }})
        log.info("[%s] device tools: %s", self.device["mac"], list(self._tool_names.values()))

    async def call_device_tool(self, safe_name: str, arguments: str) -> str:
        name = self._tool_names.get(safe_name)
        if not name:
            return f"Unknown tool {safe_name}"
        try:
            args = json.loads(arguments or "{}")
        except ValueError:
            args = {}
        try:
            resp = await self.mcp_request("tools/call", {"name": name, "arguments": args})
        except asyncio.TimeoutError:
            return "Device did not respond"
        if "error" in resp:
            return f"Error: {resp['error'].get('message', resp['error'])}"
        content = (resp.get("result") or {}).get("content") or []
        return "\n".join(c.get("text", "") for c in content if c.get("type") == "text") or "ok"
