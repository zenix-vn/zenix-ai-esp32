"""Edge TTS (mien phi, co giong tieng Viet). Tra ve MP3 -> doi sang PCM 24 kHz bang ffmpeg."""
import asyncio

import edge_tts

from ..config import DOWNLINK_RATE

VOICES = {
    "vi": ["vi-VN-HoaiMyNeural", "vi-VN-NamMinhNeural"],
    "en": ["en-US-AvaMultilingualNeural", "en-US-AndrewMultilingualNeural", "en-US-JennyNeural", "en-US-GuyNeural"],
}


async def speech_pcm(voice: str, text: str) -> bytes:
    mp3 = bytearray()
    async for chunk in edge_tts.Communicate(text, voice).stream():
        if chunk["type"] == "audio":
            mp3 += chunk["data"]
    proc = await asyncio.create_subprocess_exec(
        "ffmpeg", "-loglevel", "error", "-i", "pipe:0", "-f", "s16le", "-ac", "1", "-ar", str(DOWNLINK_RATE), "pipe:1",
        stdin=asyncio.subprocess.PIPE, stdout=asyncio.subprocess.PIPE,
    )
    pcm, _ = await proc.communicate(bytes(mp3))
    return pcm
