# Zenix AI Server

Server AI giọng nói gọn nhẹ cho thiết bị Zin (ESP32-P4). Giao thức thiết bị tương thích kiểu xiaozhi
(OTA + WebSocket + Opus + MCP), nhưng toàn bộ mã được viết mới. Mọi xử lý AI đi qua **OpenRouter**:
- nhận dạng giọng nói (STT),
- mô hình ngôn ngữ (LLM),
- giọng đọc (TTS), hoặc dùng Edge TTS miễn phí.

## Tính năng

- **Thiết bị**: mã kích hoạt 6 số, liên kết vào tài khoản, trạng thái online/đang nghe/đang nói, đổi tác nhân ngay lập tức, xem lịch sử hội thoại.
- **Tác nhân**: vai trò (prompt), ngôn ngữ Việt/Anh, chọn model LLM/STT/TTS và giọng từ danh sách OpenRouter, số lượt ghi nhớ.
- **Gọi tool thiết bị**: tool MCP mà firmware khai báo được chuyển thành function calling cho LLM, ví dụ "giảm âm lượng xuống 30".
- **Quản trị**: tài khoản admin tạo ở lần chạy đầu, thêm người dùng, OpenRouter API key (kiểm tra trước khi lưu, xem mức đã dùng), model mặc định.
- **Giao diện**: song ngữ Việt / Anh, có chế độ tối.

## Kiến trúc

```
thiết bị ──POST /xiaozhi/ota/──────────► devices.py  (đăng ký, mã kích hoạt, cấp token)
        ──WSS  /xiaozhi/v1/ ──────────► ws.py → session.py
              Opus 16 kHz → WAV → STT ─┐
              LLM stream (+ tool MCP) ─┤  providers/openrouter.py
              tách câu → TTS (song song) → Opus 24 kHz, gửi theo nhịp thời gian thực
trình duyệt ──/  /api/*───────────────► static/ (SPA) + admin.py
```

Một tiến trình Python (FastAPI). SQLite lưu tại `data/zenix.db`, khoá bí mật tại `data/secret.key`.

## Chạy cục bộ

```sh
python3.12 -m venv .venv && .venv/bin/pip install -r requirements-dev.txt
# macOS cần libopus và ffmpeg: brew install opus ffmpeg
DYLD_FALLBACK_LIBRARY_PATH=/opt/homebrew/lib .venv/bin/uvicorn app.main:app --port 8080
DYLD_FALLBACK_LIBRARY_PATH=/opt/homebrew/lib .venv/bin/python -m pytest -q
```

Mở http://localhost:8080 để tạo tài khoản quản trị. Sau đó vào **Cài đặt** và nhập OpenRouter API key.

## Triển khai (dev.zenix.vn)

```sh
rsync -a --delete --exclude .venv --exclude data --exclude __pycache__ ./ zenix-server:/opt/zenix-server/
ssh zenix-server 'cd /opt/zenix-server && docker compose up -d --build'
```

OpenLiteSpeed (vhost `zenixai`) proxy về `127.0.0.1:8020`, kể cả WebSocket `/xiaozhi/v1/`.

## Biến môi trường

| Biến | Mặc định | Ý nghĩa |
|---|---|---|
| `ZENIX_PUBLIC_URL` | `http://localhost:8080` | URL công khai, dùng để trả URL WebSocket cho thiết bị |
| `ZENIX_DATA_DIR` | `./data` | Nơi lưu SQLite và khoá bí mật |
| `ZENIX_SECRET_KEY` | tự tạo | Khoá ký cookie phiên và token thiết bị |

## Giới hạn hiện tại

- Chế độ nghe: `manual` (giữ nút để nói). Chưa có VAD phía server cho chế độ `auto`.
- Chưa có cập nhật firmware OTA, bộ nhớ dài hạn, nhận dạng giọng người nói.
