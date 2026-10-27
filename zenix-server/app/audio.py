"""Opus encode/decode va tien ich PCM."""
import io
import struct
import wave

import opuslib

from .config import DOWNLINK_RATE, FRAME_MS, UPLINK_RATE


class UplinkDecoder:
    """Giai ma khung Opus tu thiet bi ra PCM 16 kHz."""

    def __init__(self, rate: int = UPLINK_RATE):
        self.rate = rate
        self._dec = opuslib.Decoder(rate, 1)

    def decode(self, packet: bytes) -> bytes:
        # 120 ms la kich thuoc khung Opus lon nhat.
        return self._dec.decode(packet, self.rate * 120 // 1000)


class DownlinkEncoder:
    """Cat PCM 24 kHz thanh khung 60 ms va ma hoa Opus."""

    def __init__(self, rate: int = DOWNLINK_RATE):
        self.rate = rate
        self.frame_samples = rate * FRAME_MS // 1000
        self._enc = opuslib.Encoder(rate, 1, opuslib.APPLICATION_AUDIO)

    def encode(self, pcm: bytes) -> list[bytes]:
        frame_bytes = self.frame_samples * 2
        if len(pcm) % frame_bytes:
            pcm += b"\x00" * (frame_bytes - len(pcm) % frame_bytes)
        return [self._enc.encode(pcm[i : i + frame_bytes], self.frame_samples) for i in range(0, len(pcm), frame_bytes)]


def pcm_to_wav(pcm: bytes, rate: int = UPLINK_RATE) -> bytes:
    buf = io.BytesIO()
    with wave.open(buf, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(pcm)
    return buf.getvalue()


def rms(pcm: bytes) -> float:
    n = len(pcm) // 2
    if n == 0:
        return 0.0
    samples = struct.unpack(f"<{n}h", pcm[: n * 2])
    return (sum(s * s for s in samples) / n) ** 0.5
