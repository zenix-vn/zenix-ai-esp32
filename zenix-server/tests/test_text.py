from app.text import SentenceSplitter, clean_for_speech


def test_splits_on_sentence_end_and_keeps_decimals():
    s = SentenceSplitter(first_soft_min=1000)
    out = []
    for piece in ["Giá là 3.5 triệu. ", "Bạn muốn mua ", "không? Cảm ơn"]:
        out += s.feed(piece)
    out += s.flush()
    assert out == ["Giá là 3.5 triệu.", "Bạn muốn mua không?", "Cảm ơn"]


def test_first_sentence_cut_early_at_comma():
    s = SentenceSplitter(first_soft_min=10)
    assert s.feed("Xin chào bạn thân mến, hôm nay ") == ["Xin chào bạn thân mến,"]
    assert s.feed("trời đẹp, đi chơi nhé. ") == ["hôm nay trời đẹp, đi chơi nhé."]


def test_clean_for_speech():
    assert clean_for_speech("**Chào** bạn 😀 [link](http://x)") == "Chào bạn link"
    assert clean_for_speech("😀") == ""


def test_pick_voice_by_language():
    from app.session import pick_voice
    assert pick_voice("edge", "vi", "") == "vi-VN-HoaiMyNeural"
    assert pick_voice("edge", "en", "Kore") == "en-US-AvaMultilingualNeural"
    assert pick_voice("edge", "en", "vi-VN-NamMinhNeural") == "vi-VN-NamMinhNeural"
    assert pick_voice("openrouter", "vi", "") == "Kore"
    assert pick_voice("openai", "vi", "") == "alloy"
