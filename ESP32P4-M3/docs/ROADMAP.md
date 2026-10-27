# Kế hoạch phát triển firmware Zenix AI cho JC-ESP32P4-M3-DEV

Mục tiêu: biến mạch thành **thiết bị trợ lý giọng nói hoàn chỉnh** chạy với server AI `dev.zenix.vn` (server riêng của Zenix, OpenRouter, gói cước). Phần cứng chi tiết: [BOARD.md](BOARD.md).

## 1. Hiện trạng (đã chạy ✅)

| Mảng | Đã có |
|---|---|
| Nền | ESP-IDF v6.0.3, bootloader nhỏ, bảng phân vùng tùy chỉnh (factory 6 MB + storage 4 MB) |
| Mạng | Wi-Fi qua C6 (ESP-Hosted), AP cài đặt `Zin-Setup` + captive DNS, trang quản lý `http://ai-setup.local`, quét Wi-Fi, lưu NVS |
| Server | OTA/kích hoạt mã 6 số, WebSocket giao thức xiaozhi, kết nối bền vững, tự nối lại (backoff 2–60 s), nhận lệnh MCP |
| Âm thanh | ES8311, Opus 16 kHz lên / 24 kHz xuống (khung 60 ms), nói bằng cách **giữ nút BOOT** (chế độ thủ công) |
| MCP | `self.get_device_status`, `self.audio_speaker.set_volume` |
| LED trạng thái | Firmware sẵn sàng (PWM hoặc WS2812), **mạch chưa có đèn dùng được**: D1 là đèn nguồn |

Giới hạn hiện nay: chỉ nói khi giữ nút; không có từ khóa đánh thức; không cập nhật firmware từ xa (không có phân vùng OTA); firmware C6 lệch phiên bản; không dùng Ethernet/màn hình/thẻ nhớ/RS485; chưa có kiểm thử tự động cho firmware.

## 2. Nguyên tắc

1. **Mỗi pha nạp được và chạy được độc lập.** Không để mạch rơi vào trạng thái hỏng giữa chừng (đặc biệt OTA).
2. **Server làm phần nặng** (STT, LLM, TTS). Thiết bị lo âm thanh, mạng, an toàn, hiển thị trạng thái.
3. **Chi phí và gói cước** do server quản lý; thiết bị chỉ báo lỗi dễ hiểu (đã có alert “thiếu gói/API key”).
4. Kiểm chứng trên mạch thật trước khi đánh dấu xong; ghi cả cái chưa kiểm chứng.

## 3. Các pha

Thứ tự theo giá trị với người dùng và rủi ro. Ước lượng thô theo ngày làm việc của một người.

### Pha 1 — Nền tảng ổn định (4–6 ngày)

Mục tiêu: mạch chạy bền, cập nhật được từ xa, an toàn.

| # | Việc | Ghi chú |
|---|---|---|
| 1.1 | **Đổi bảng phân vùng sang OTA kép**: `nvs`, `otadata`, `ota_0` (~6 MB), `ota_1` (~6 MB), bỏ/thu nhỏ `storage` | Việc **phải làm đầu tiên**: đổi bảng phân vùng cần nạp lại qua cáp, sau đó mới cập nhật từ xa được |
| 1.2 | **OTA firmware qua server**: OTA response trả `firmware.url` + phiên bản; thiết bị tải về, kiểm tra hash, đổi phân vùng, **tự quay về bản cũ nếu không lên được** (`esp_ota_mark_app_valid`) | Server: thêm trang tải bản firmware ở admin (chỉ admin), lưu file + SHA-256, kênh ổn định/thử nghiệm |
| 1.3 | **Cập nhật firmware ESP32-C6** (slave 2.3.0 → khớp host 2.13) bằng `esp_hosted` OTA | Hết cảnh báo lệch phiên bản, giảm nguy cơ timeout RPC |
| 1.4 | **Watchdog + báo lỗi**: task watchdog, ghi lý do khởi động lại (`esp_reset_reason`), gửi log lỗi lên server | |
| 1.5 | **Bảo mật**: bật Secure Boot v2 + mã hóa flash ở bản phát hành (không bật khi còn phát triển), bỏ token WebSocket mặc định `test-token`, đặt mật khẩu cho trang quản lý | Trang quản lý hiện **không có mật khẩu**: ai cùng Wi-Fi đều đổi được cấu hình |
| 1.6 | **Cấu hình phát hành** (`sdkconfig.release`), tắt log chi tiết, đặt version từ git | |

Tiêu chí xong: đổi phiên bản firmware từ trang admin, mạch tự cập nhật và tự quay về nếu bản mới lỗi; không còn cảnh báo C6.

### Pha 2 — Trải nghiệm giọng nói (7–10 ngày)

Mục tiêu: nói chuyện tự nhiên, không phải giữ nút.

| # | Việc | Ghi chú |
|---|---|---|
| 2.1 | **Từ khóa đánh thức** bằng **ESP-SR WakeNet** (chạy trên P4, thư viện Espressif) | Cần chọn từ (mẫu có sẵn “Hi ESP”, từ tùy chỉnh “Hey Zenix” cần huấn luyện/đặt hàng mô hình). Tạm thời dùng mẫu có sẵn |
| 2.2 | **Chế độ tự động dừng nghe (VAD)**: ESP-SR VAD, hoặc server chủ động báo kết thúc câu | Cho phép 3 chế độ: giữ nút / bấm một lần / từ khóa. Chọn ở trang quản lý |
| 2.3 | **Ngắt lời (barge-in)**: khi đang nói mà người dùng nói chen, gửi `abort` | Cần **AEC phần mềm** (ESP-SR AFE có AEC) vì mic và loa chung codec, không có đường tham chiếu phần cứng. **Rủi ro: trung bình-cao**, cần thử nhiều mức âm lượng |
| 2.4 | **Khử ồn, AGC** (ESP-SR AFE) | |
| 2.5 | **Âm báo**: tiếng “ding” khi bắt đầu nghe, báo lỗi, báo kết nối (nhúng sẵn vài file Opus/PCM nhỏ) | Thiết bị không có màn hình/đèn nên âm thanh là kênh phản hồi chính |
| 2.6 | **Âm lượng, tắt mic** bằng lệnh nói (“tăng âm lượng”) qua MCP, đã có nửa phần | |

Tiêu chí xong: nói “từ khóa” rồi hỏi liên tục, không chạm nút; ngắt lời được; đo độ trễ từ lúc dứt lời đến lúc nghe tiếng đầu tiên **dưới 2 giây** trên Wi-Fi nhà.

### Pha 3 — Mạng đa đường và khả năng chịu lỗi (3–4 ngày)

| # | Việc | Ghi chú |
|---|---|---|
| 3.1 | **Ethernet RJ45** (IP101, RMII): dùng khi cắm dây, tự chuyển về Wi-Fi khi rút | Đổi nút bấm khỏi GPIO35 (trùng TXD1) |
| 3.2 | **Chế độ cài đặt dự phòng**: không vào được Wi-Fi sau N lần → tự bật `Zin-Setup`; giữ nút BOOT 5 giây khi khởi động → xóa cấu hình | Hiện cần cài lại mới vào lại được setup khi sai mật khẩu |
| 3.3 | **mDNS + trang quản lý chạy cả khi ở chế độ STA** (đã có), thêm mã PIN | |
| 3.4 | **Đồng bộ giờ (SNTP)**, múi giờ | Cần cho OTA có thời hạn, log |
| 3.5 | **Ghép Bluetooth (BLE) để cấu hình Wi-Fi** từ điện thoại (tùy chọn, dùng C6) | |

### Pha 4 — Công cụ thiết bị cho AI (MCP) (5–7 ngày)

Để AI điều khiển thế giới thật qua giọng nói.

| # | Công cụ MCP (đề xuất) | Dùng phần cứng |
|---|---|---|
| 4.1 | `self.get_device_status` mở rộng: nhiệt độ chip, RAM/PSRAM, RSSI, uptime, phiên bản | đã có nền |
| 4.2 | `self.audio_speaker.set_volume`, `self.audio_mic.mute` | đã có / ES8311 |
| 4.3 | `self.gpio.set/get` (có **danh sách chân cho phép** `1,2,3,4,5,20,32,33,46,47`) | header JP1: điều khiển rơ-le, đọc cảm biến |
| 4.4 | `self.rs485.transact` (gửi khung, đọc phản hồi, hỗ trợ Modbus RTU cơ bản) | MAX485 trên UART1 (GPIO26/27) |
| 4.5 | `self.i2c.read` cho cảm biến gắn `CN3` | I2C chung |
| 4.6 | `self.reboot`, `self.set_wake_word`, `self.get_battery` | khi xác nhận được chân đo pin ❓ |

Server: mỗi công cụ cần mô tả rõ để mô hình biết khi nào gọi; ghi nhật ký mọi lệnh; **mặc định chỉ chủ thiết bị được dùng**, có công tắc “cho phép AI điều khiển phần cứng” theo từng tác nhân.

### Pha 5 — Phần cứng mở rộng (tùy chọn, 8–15 ngày)

Chỉ làm khi có nhu cầu sản phẩm. Làm riêng từng mục, không phụ thuộc nhau.

| # | Việc | Rủi ro |
|---|---|---|
| 5.1 | **Màn hình MIPI-DSI + cảm ứng GT911** + LVGL: hiển thị chữ trả lời, mã kích hoạt, trạng thái | Phụ thuộc loại màn đang có; cần panel và driver đúng |
| 5.2 | **Camera MIPI-CSI** → gửi ảnh cho mô hình đa phương thức (“đây là cái gì?”) | Server cần hỗ trợ ảnh trong giao thức; chi phí token lớn hơn |
| 5.3 | **Thẻ nhớ TF**: lưu âm thanh, nhạc, cache | Nguồn thẻ qua LDO4, kiểm tra GPIO45/R10 trước |
| 5.4 | **Đèn trạng thái**: gắn **WS2812 vào GPIO20** (hoặc dùng màn hình) | Firmware sẵn sàng; chỉ cần phần cứng |
| 5.5 | **Đo pin** (IP5306 + chia áp), báo pin yếu | Chân đo chưa chắc chắn ❓ |
| 5.6 | **Chế độ ngủ tiết kiệm điện** | Wi-Fi qua C6 khiến tiết kiệm điện phức tạp; cân nhắc sau cùng |

### Pha 6 — Chất lượng và vận hành (liên tục)

| # | Việc |
|---|---|
| 6.1 | **Kiểm thử firmware**: Unity chạy trên mạch (codec, Opus, xử lý JSON, NVS), kịch bản **end-to-end** với server thật (giả lập “mạch ảo” bằng script WebSocket đã có trong `tests/`) |
| 6.2 | **CI**: build IDF + kiểm tra cảnh báo + chạy test server mỗi lần đẩy mã |
| 6.3 | **Đo lường**: độ trễ (STT/LLM/TTS), tỉ lệ rớt kết nối, lý do khởi động lại, ghi vào server để vẽ biểu đồ ở trang admin |
| 6.4 | **Tài liệu**: hướng dẫn người dùng (cài đặt lần đầu, kích hoạt, khắc phục sự cố), tài liệu giao thức thiết bị ↔ server |
| 6.5 | **Dọn dẹp server**: xóa hẳn stack xiaozhi cũ ở `/opt/zenix-ai` khi chắc chắn không cần, sao lưu CSDL định kỳ |

## 4. Phần việc phía server (dev.zenix.vn) đi kèm

| Pha | Việc server |
|---|---|
| 1 | Lưu và phát hành firmware (tải lên, hash, kênh, quản lý phiên bản theo thiết bị); API nhận log lỗi |
| 2 | Hỗ trợ chế độ nghe tự động (kết thúc câu theo VAD/ngưng tiếng); ngắt lời (hủy đang phát, giữ ngữ cảnh) |
| 3 | Hiển thị lý do ngoại tuyến, thời điểm thấy thiết bị lần cuối |
| 4 | Quản lý công cụ MCP theo tác nhân; nhật ký lệnh; quyền theo chủ thiết bị |
| 5 | Nhận ảnh (camera), phát lại nhạc/âm báo từ thư viện |
| chung | Giới hạn tốc độ và hạn mức theo gói; cảnh báo chi phí; sao lưu |

## 5. Rủi ro và cách giảm

| Rủi ro | Mức | Giảm |
|---|---|---|
| OTA hỏng làm mạch “brick” | Cao | OTA kép + rollback tự động, thử kỹ trên mạch phụ trước khi phát hành |
| AEC kém, tự nghe lại tiếng mình | Trung bình-cao | Dùng ESP-SR AFE; hạ âm lượng; dự phòng chế độ “im lặng khi đang nói” (nghe sau khi nói xong) |
| Firmware C6 lệch | Trung bình | Làm sớm (pha 1.3) |
| Chân GPIO53 dùng hai việc (loa/đo pin) | Thấp | Chỉ dùng cho loa, đo pin chuyển sang chân khác |
| Ethernet đụng nút BOOT (GPIO35) | Thấp | Đổi nút bấm trước khi bật Ethernet |
| Sơ đồ ảnh không chính thức | Thấp | Kiểm tra bằng đồng hồ vạn năng trước khi nối tải lớn |
| Chi phí AI khi mở đăng ký | Trung bình | Đã chặn người không có gói/khóa; thêm hạn mức theo ngày |
| Không có màn hình/đèn nên người dùng khó biết trạng thái | Trung bình | Âm báo (pha 2.5), gắn WS2812 (5.4), hoặc màn hình (5.1) |

## 6. Việc tiếp theo đề xuất

1. **Pha 1.1 + 1.2** (OTA kép + OTA qua server) — mở khóa mọi việc sau, vì từ đó nạp firmware từ xa, không cần cắm cáp.
2. **Pha 1.3** (cập nhật C6) và **1.5** (mật khẩu trang quản lý).
3. **Pha 2.1–2.2** (từ khóa đánh thức, tự dừng nghe): thay đổi lớn nhất về trải nghiệm.

Việc cần bạn quyết định trước khi bắt đầu pha 2 và 5:
- Từ khóa đánh thức mong muốn (“Hey Zenix”?) và có chấp nhận dùng từ mẫu có sẵn lúc đầu không.
- Có định gắn **màn hình / camera / WS2812** không (quyết định nên làm 5.1, 5.2, 5.4 hay bỏ).
- Mạch dùng một mình trong nhà (không cần Secure Boot ngay) hay đem bán/triển khai nhiều (cần Secure Boot, mã hóa flash, nạp hàng loạt).
