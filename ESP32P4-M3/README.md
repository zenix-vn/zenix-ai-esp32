# Zenix AI client — ESP32-P4-M3

> Tài liệu: [phần cứng chi tiết (sơ đồ, chân, nguồn)](docs/BOARD.md) · [kế hoạch phát triển firmware](docs/ROADMAP.md)

Firmware mẫu nói chuyện với server AI theo giao thức **xiaozhi** (WebSocket + Opus + MCP).
Giữ nút BOOT để nói, nhả ra để gửi; AI trả lời qua loa.

## Phần cứng đã xác minh

| Khối | Chi tiết |
|---|---|
| SoC | ESP32-P4 rev v3.2, 400 MHz, flash 16 MB (Boya), PSRAM 32 MB |
| Wi-Fi | ESP32-C6 qua SDIO (ESP-Hosted), CLK18 CMD19 D0-D3 14-17, reset GPIO54 |
| Codec | ES8311 @ I2C 0x18 (SDA 7, SCL 8); I2S MCLK13 BCLK12 WS10 DOUT9 DIN48; PA GPIO11; pin GPIO53 (ADC) |
| Nút | BOOT = GPIO35 |
| Console | UART0 GPIO37/38 qua CH340 (`/dev/cu.usbserial-*`) |

## Cài đặt môi trường

ESP-IDF v6.0.3 đã cài tại `~/esp/esp-idf-v6.0`. Mỗi terminal mới:

```sh
. ~/esp/esp-idf-v6.0/export.sh
```

## Cài đặt Wi-Fi (không cần nạp lại firmware)

Thiết bị phát Wi-Fi mở **Zin-Setup** khi:
- chưa có Wi-Fi nhà, hoặc
- không kết nối được Wi-Fi đã lưu trong 30 giây, hoặc
- bạn **giữ nút BOOT trong lúc cấp nguồn/reset**.

1. Điện thoại kết nối Wi-Fi **Zin-Setup**. Trang cài đặt thường tự bật lên; nếu không, mở `http://ai-setup.local` hoặc `http://192.168.4.1`.
   Địa chỉ 192.168.4.1 **chỉ tồn tại khi điện thoại đang nối vào Zin-Setup**.
2. Tab **Wi-Fi**: trang tự quét mạng xung quanh, chọn Wi-Fi nhà, nhập mật khẩu, bấm **Kết nối**.
3. Thiết bị thử kết nối ngay (tối đa 20 giây). Thành công thì lưu lại và khởi động lại. Sai mật khẩu thì báo lỗi ngay trên trang.
   Nếu điện thoại bị rớt khỏi Zin-Setup trong lúc này, đó thường là dấu hiệu đã kết nối thành công
   (AP phải đổi kênh theo router).

## Trang quản lý trong mạng nhà

Khi đã vào mạng nhà, trang quản lý mở ở **`http://ai-setup.local`** (mDNS).
Tên phụ theo MAC là `http://zin-xxxx.local`, ví dụ `http://zin-fe18.local`; dùng tên này khi có nhiều thiết bị trong cùng mạng.
Một số điện thoại Android không phân giải được `.local`, khi đó dùng IP.
Cũng mở được bằng IP (xem trên router hoặc log). Trang có:
- **Wi-Fi**: mạng hiện tại, quét và đổi mạng.
- **Server**: OTA URL / WebSocket URL / token. Mặc định `https://dev.zenix.vn/xiaozhi/ota/`.
- **Thiết bị**: trạng thái kết nối AI, MAC, firmware, RAM, âm lượng, khởi động lại, xoá cấu hình.
- **Mã kích hoạt** hiện ở đầu trang khi server yêu cầu.

Cấu hình lưu trong NVS (namespace `cfg`). Giá trị mặc định lấy từ `idf.py menuconfig → Zenix AI Client`.
Trang quản lý **không có mật khẩu**: ai cùng mạng LAN cũng mở được.

## Đèn LED trạng thái

Cấu hình ngay trên trang quản lý (`http://ai-setup.local` → tab **Thiết bị** → **Đèn LED trạng thái**), không cần nạp lại firmware:
- **Loại:** Tắt / LED đơn màu (sáng ở mức cao) / LED đơn màu (sáng ở mức thấp) / LED RGB WS2812.
- **Chân:** chỉ cho chọn các chân an toàn: 1–5, 20, 32, 33, 46, 47 (các chân tự do trên header JP1, xem docs/BOARD.md). Mọi chân khác (SDIO Wi-Fi, codec, Ethernet, RS485, USB, thẻ nhớ, nút BOOT…) bị chặn.
- **Dò tự động:** nút "Dò LED tự động" cho từng chân nháy ~3 giây để tìm chân nối LED. Mạch JC-ESP32P4-M3-DEV **không có LED người dùng** (D1 là đèn nguồn), cần gắn LED ngoài.
- **Thử:** mỗi khi đổi loại hoặc chân, LED nháy trắng 3 giây. Cách này giúp dò ra chân nối LED.

| Trạng thái | RGB | LED đơn màu |
|---|---|---|
| Khởi động | trắng, thở | thở |
| Kết nối Wi-Fi | xanh lơ, nháy nhanh | nháy nhanh |
| Phát Zin-Setup | xanh dương, nháy chậm | nháy chậm |
| Chờ kích hoạt | tím, nháy đôi | nháy đôi |
| Kết nối server | cam, nháy nhanh | nháy nhanh |
| Mất kết nối server | cam đỏ, nháy đôi chậm | nháy đôi chậm |
| Sẵn sàng | xanh lá, sáng mờ | sáng mờ 15% |
| Đang nghe | xanh lơ, sáng | sáng |
| Đang nghĩ | tím, thở | thở |
| Đang nói | xanh dương, thở nhanh | thở nhanh |
| Lỗi | đỏ, nháy nhanh 3 giây | nháy nhanh |

Mã nguồn: `main/status_led.c`. Bảng hiệu ứng nằm trong `PATTERNS`.

## Build và nạp

```sh
idf.py build
idf.py -p /dev/cu.usbserial-1110 -b 460800 flash monitor   # Ctrl+] để thoát
```

CH340 không ổn định ở 921600. Nếu nạp lỗi, dùng `-b 115200`.

## Lần chạy đầu: kích hoạt

Log sẽ in:

```
W (..) ota:   MA KICH HOAT: 123456
```

Mã này cũng hiện trên trang quản lý của thiết bị. Vào https://dev.zenix.vn → Agent → **Add device** → nhập mã.
Thiết bị tự nhận ra khi đã kích hoạt và chuyển sang trạng thái sẵn sàng.

## Luồng hoạt động

1. Khởi động: quét I2C, khởi tạo ES8311 + Opus, kết nối Wi-Fi.
2. Có Wi-Fi: bật trang quản lý + mDNS. Không có: vào chế độ cài đặt Zin-Setup.
3. `POST` OTA URL (header `Device-Id` = MAC, `Client-Id` = UUID lưu NVS) → nhận `websocket.url`/`token`
   hoặc `activation.code`.
4. Nhấn nút: mở WebSocket (`Authorization: Bearer <token>`, `Protocol-Version: 1`), gửi `hello`,
   chờ `hello` của server (lấy `session_id`), gửi `listen start` (mode `manual`).
5. Trong lúc giữ nút: mỗi 60 ms mã hoá 960 mẫu 16 kHz thành 1 khung Opus và gửi dạng binary.
6. Nhả nút: gửi `listen stop`. Server trả `stt` (câu bạn nói), `llm` (cảm xúc), `tts start` →
   các khung Opus → `tts stop`. Thiết bị giải mã thẳng ra 16 kHz và phát.
7. Nhấn nút khi AI đang nói: gửi `abort` và ngắt phát.
8. MCP: server gọi `initialize`, `tools/list`, `tools/call`. Thiết bị có 2 tool mẫu:
   `self.get_device_status` và `self.audio_speaker.set_volume` (thử nói "giảm âm lượng xuống 30").

## Cấu trúc mã

| File | Vai trò |
|---|---|
| `main/main.c` | Máy trạng thái (idle/connecting/listening/speaking), nút nhấn, task mic & loa |
| `main/xz_protocol.c` | WebSocket client, hello handshake, ghép khung, gửi JSON/Opus |
| `main/ota_activate.c` | Gọi OTA, lấy cấu hình server, luồng mã kích hoạt |
| `main/mcp.c` | MCP server tối thiểu (JSON-RPC 2.0) — thêm tool mới tại `list_tools`/`call_tool` |
| `main/audio.c` | I2S + ES8311 (esp_codec_dev), Opus encode/decode (esp_audio_codec) |
| `main/net_wifi.c` | Wi-Fi STA qua ESP32-C6 (esp_wifi_remote + esp_hosted) |
| `main/board.c` | Bus I2C (kèm quét chẩn đoán), nút BOOT |
| `main/settings.c` | Cấu hình trong NVS (Wi-Fi, server, âm lượng) |
| `main/web_server.c` | Trang quản lý + REST API (`/api/status`, `/api/scan`, `/api/wifi`, `/api/config`, `/api/reboot`, `/api/reset`) |
| `main/web/index.html` | Giao diện trang quản lý (nhúng vào firmware) |
| `main/captive_dns.c` | DNS bắt mọi tên miền về 192.168.4.1 ở chế độ cài đặt |
| `main/status_led.c` | Đèn LED trạng thái: PWM đơn màu hoặc WS2812, hiệu ứng theo trạng thái |

## Lưu ý

- Firmware ESP-Hosted trên C6 đang là 2.3.0, host là 2.13. Hiện chạy được; nếu gặp lỗi RPC timeout
  thì cần cập nhật firmware C6.
- Chưa có: wake word, AEC, màn hình, cập nhật OTA firmware (trường `firmware` từ server chỉ được log).
