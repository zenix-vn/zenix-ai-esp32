"""Xu ly van ban cho giong noi: tach cau tu stream LLM, loai bo ky hieu khong doc duoc."""
import re

_TERMINALS = ".!?。！？\n"
_SOFT = ",;:，；："
_EMOJI = re.compile("[\U0001F000-\U0001FAFF☀-➿️]")
_MARKDOWN = re.compile(r"[*_`#>|~]+")


class SentenceSplitter:
    """Nhan tung manh van ban, tra ve cac cau hoan chinh de doc ngay (giam do tre)."""

    def __init__(self, first_soft_min: int = 40):
        self.buf = ""
        self.first = True
        self.first_soft_min = first_soft_min

    def feed(self, text: str) -> list[str]:
        self.buf += text
        out = []
        while True:
            cut = self._find_cut()
            if cut < 0:
                break
            sentence, self.buf = self.buf[: cut + 1], self.buf[cut + 1 :]
            if clean_for_speech(sentence):
                out.append(sentence.strip())
                self.first = False
        return out

    def flush(self) -> list[str]:
        rest, self.buf = self.buf.strip(), ""
        return [rest] if clean_for_speech(rest) else []

    def _find_cut(self) -> int:
        for i, ch in enumerate(self.buf):
            # Dau cham trong so (3.5) hoac viet tat khong phai ket thuc cau.
            if ch in _TERMINALS and (i + 1 < len(self.buf) and (self.buf[i + 1].isspace() or ch == "\n")):
                return i
            # Cau dau tien: cat som o dau phay de bat dau noi nhanh hon.
            if self.first and ch in _SOFT and i >= self.first_soft_min and i + 1 < len(self.buf):
                return i
        return -1


def clean_for_speech(text: str) -> str:
    text = _EMOJI.sub("", text)
    text = _MARKDOWN.sub("", text)
    text = re.sub(r"\[([^\]]*)\]\([^)]*\)", r"\1", text)  # link markdown
    return re.sub(r"\s+", " ", text).strip()
