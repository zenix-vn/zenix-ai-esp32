# JC-ESP32P4-M3-DEV — tài liệu phần cứng

Bo mạch phát triển **Guition JC-ESP32P4-M3-DEV** (ký hiệu in trên mạch: `JC-ESP32P4-M3-DEV`, module `JC-ESP32P4-M3`).
Dùng làm thiết bị đầu cuối cho hệ thống AI **Zenix AI** (server `dev.zenix.vn`).

## Nguồn thông tin và độ tin cậy

| Nguồn | Dùng cho |
|---|---|
| Sơ đồ nguyên lý của nhà sản xuất (8 trang, ảnh PNG) lưu ở [schematics/](schematics/), lấy từ repo [unofficial_guition_esp32p4_repo](https://github.com/p1ngb4ck/unofficial_guition_esp32p4_repo) (không chính thức) | Chân, linh kiện, mạch nguồn |
| File ESPHome mẫu của cùng repo | Đối chiếu chân Ethernet, SPI, I2S |
| **Đo trên mạch thật** (build firmware, log, quét I2C) | Mục đánh dấu ✅ |

Quy ước: ✅ = đã kiểm chứng trên mạch thật; 📄 = đọc từ sơ đồ, chưa thử trên mạch; ❓ = chưa chắc, cần kiểm tra trước khi dựa vào.

## 1. Tổng quan

| Thành phần | Chi tiết |
|---|---|
| Module | ESP32-P4NRW32 trong module Guition, ✅ chip **rev v3.2**, 2 nhân RISC-V 400 MHz |
| Flash | 16 MB (Boya), ✅ cần `CONFIG_SPI_FLASH_SUPPORT_BOYA_CHIP` |
| PSRAM | ✅ 32 MB, chạy 200 MHz |
| Wi-Fi / BLE | **ESP32-C6** (đồng xử lý) nối với P4 qua **SDIO** (ESP-Hosted). P4 không có Wi-Fi riêng. ✅ chạy tốt; firmware slave 2.3.0 cũ hơn host 2.13 (xem [ROADMAP.md](ROADMAP.md) mục C6) |
| Ăng-ten | Mạch nối ăng-ten sứ (`ANT1`, `LAN_OUT` qua L4 2,2nH); trên ảnh có chân giắc U.FL cạnh module. 📄 |
| Cổng | 3 cổng USB-C, 1 RJ45, khe thẻ TF, khe pin, loa, header 26 chân |

## 2. Nguồn

📄 (trang 1 và 2 của sơ đồ)

- **USB5V_IN**: cấp từ cổng USB-C hoặc cổng 4 chân `CN2` (VIN). Có MOSFET AO3401 (Q2) chống ngược cực và TVS SMBJ6.0CA.
- **VCC3V3**: buck **TLV62569** (5V → 3,3V), cấp cho P4, codec, PHY, thẻ nhớ.
- **Pin 1S Li-ion**: IC **IP5306** (sạc + boost), đầu nối `CN4` (BAT+/BAT−), nút `SW3` (KEY) của IP5306. Rail **VOUT-BAT** cấp cho bộ khuếch đại loa NS4150 và mạch nền màn hình.
- ❓ Trang 2 có nhánh `GPIO53 — R55 0Ω — R52 68k / R57 100k — BAT+`, tức là chia áp đo pin vào GPIO53. Nhưng GPIO53 cũng là `PA_CTRL` (bật loa) ở trang 1. Firmware hiện dùng GPIO53 điều khiển loa và chạy tốt; **đừng dùng GPIO53 để đọc pin** trước khi kiểm tra kỹ.
- **D1** (đèn đỏ 0603, cạnh tụ C23): ✅ chỉ là **đèn báo nguồn**, mắc nối tiếp R21 5,1kΩ từ nguồn 5V xuống GND. Không nối vào GPIO nào, **phần mềm không điều khiển được**.

## 3. Âm thanh

📄 + ✅ (trang 1, 3)

```
Mic (MK1, analog) ─► ES8311 (MIC1P/MIC1N) ─► I2S ─► ESP32-P4
ESP32-P4 ─► I2S ─► ES8311 (DAC OUTP/OUTN) ─► NS4150 (D, 3W) ─► loa CN1
```

| Tín hiệu | GPIO | Ghi chú |
|---|---|---|
| I2C SDA | **7** | ✅ chung bus với cảm ứng và đầu nối `CN3` |
| I2C SCL | **8** | ✅ kéo lên 3V3 qua R65/R71 (5,1kΩ) |
| I2S MCLK | **13** | ✅ |
| I2S BCLK (SCLK) | **12** | ✅ |
| I2S WS (LRCK) | **10** | ✅ |
| I2S DOUT (P4 → DAC, chân `DSDIN` của codec) | **9** | ✅ |
| I2S DIN (ADC → P4, chân `ASDOUT` của codec) | **11** | ✅ |
| PA_CTRL (chân STD của NS4150) | **53** | ✅ mức cao = bật loa |
| ES7210_SDOUT | **48** | ❓ có net `ES7210_SDOUT` nhưng mạch tôi kiểm tra **chỉ thấy ES8311**; chưa rõ có gắn ES7210 (ADC 4 mic) không |

- **ES8311**: I2C `0x18` ✅. Chỉ **1 mic analog** (MIC1P/MIC1N); loa mono.
- Mic và loa **cùng một codec** nên **không có tách tiếng vọng bằng phần cứng**; phải xử lý bằng phần mềm (AEC) nếu muốn ngắt lời khi robot đang nói (xem ROADMAP, pha 2).
- Nguồn analog của codec tách riêng (`ADC_GND`, `Codec_3V3` qua R9 0Ω).

## 4. Mạng

### Wi-Fi + Bluetooth (ESP32-C6)

✅ Nối P4 ↔ C6 qua **SDIO slot 1**:

| Tín hiệu | GPIO |
|---|---|
| CLK | 18 |
| CMD | 19 |
| D0 / D1 / D2 / D3 | 14 / 15 / 16 / 17 |
| Reset C6 | 54 |

Firmware dùng `esp_hosted` + `esp_wifi_remote`, MAC Wi-Fi ✅ `7c:0c:5f:d8:fe:18`. Chân phụ C6 ra header: `C6_U0RXD`, `C6_U0TXD`, `C6_IO9`, `C6_CHIP_PU` (dùng để nạp lại firmware C6, xem ROADMAP).

### Ethernet (RJ45)

📄 (trang 7, chưa thử)

- PHY **IP101** (U4), thạch anh 25 MHz (X1), địa chỉ PHY `1`, giao tiếp **RMII**, đầu RJ45 có biến áp và 2 LED liên kết/hoạt động ngay trên giắc.

| Tín hiệu | GPIO |
|---|---|
| RMII_TXEN | 49 |
| RMII_CLK (50 MHz vào từ PHY) | 50 |
| PHY_RSTN / nguồn | 51 |
| MDIO | 52 |
| MDC | 31 |
| RXD0 / RXD1 / CRS_DV | 29 / 30 / 28 |
| TXD0 / TXD1 | 34 / **35** |

⚠️ **GPIO35 vừa là nút BOOT vừa là TXD1 của Ethernet.** Khi bật Ethernet, không dùng nút BOOT làm nút bấm để nói. Cần chọn nút khác (xem ROADMAP, mục Nút bấm).

## 5. USB và nạp firmware

📄 + ✅ (trang 5)

| Cổng | Chức năng | Ghi chú |
|---|---|---|
| `USB1` | **CH340** (USB-UART) | UART0 của P4: TX=GPIO37, RX=GPIO38. ✅ `/dev/cu.usbserial-1110`. Mạch tự reset/BOOT qua DTR/RTS (Q3, Q4) vào `BOOTMODE` (GPIO35) và `CHIP_PU` |
| `USB2` (“Full speed USB”) | USB trực tiếp của P4, chân `USB1P1_P/N` | ✅ chính là cổng USB-Serial-JTAG `/dev/cu.usbmodem11101` (đang nạp firmware bằng cổng này) |
| `USB3` (“High speed USB”) | USB high-speed của P4 (`ESP_USB_P/N`) | 📄 dùng làm USB host/device tốc độ cao, chưa thử |

- Baud nạp qua CH340: ✅ **921600 lỗi**, **460800 chạy được**, 115200 chắc chắn.
- Nút: `SW1` = **BOOT** (GPIO35), `SW2` = **RESET** (CHIP_PU).

## 6. Thẻ nhớ TF

📄 (trang 3, chưa thử)

- Giao tiếp **SDMMC slot 0**, chân cố định của P4: DATA0=39, DATA1=40, DATA2=41, DATA3=42, CLK=43, CMD=44. Chế độ 4 bit.
- Nguồn thẻ lấy từ **LDO kênh 4** của P4 (`ESP_LDO_VO4`, ví dụ ESPHome đặt 2,7V ✅ tham khảo) qua công tắc AO3401 (Q1). GPIO45 nối cổng Q1 qua R10 nhưng R10 ghi **NC** (không hàn) nên nhiều khả năng nguồn thẻ luôn bật. ❓
- Kéo lên bằng điện trở 10k/5,1k trên mạch.

## 7. Màn hình, cảm ứng, camera

📄 (trang 2, 4, 6)

- **MIPI-DSI**: đầu `J2` hoặc `J3` (FPC), 2 lane (`DSI_A_DATA0/1`, `DSI_A_CLK`). Nguồn PHY lấy từ LDO kênh 3 (2,5V).
- **MIPI-CSI** (camera): FPC còn lại, 2 lane (`CSI_A_DATA0/1`, `CSI_A_CLK`).
- **Cảm ứng** `FPC1` (6 chân, GT911 theo ví dụ ESPHome): `RTC_DAT/SDA1` và `RTC_CLK/SCL1` nối với I2C chung, `TOUCH_INT` = **GPIO20**, `TOUCH_RST` = **GPIO21**.
- **Đèn nền** `CN5`: mạch tăng áp **MP3202** (IC1), bật/PWM bằng `LCD_PWM` = **GPIO22** (hoặc qua thanh ghi của bo màn hình, như ví dụ ESPHome).

Firmware hiện tại **không** dùng màn hình, camera.

## 8. RS485 / UART

📄 (trang 5, 8, chưa thử)

- **MAX485** (U8), UART1 của P4: **TX = GPIO26, RX = GPIO27** (nhãn `TX1`/`RX1` trên chip).
- **Tự đảo chiều DE/RE** bằng mạch cổng 74LVC1G132 (U7) + transistor 8550 (U9): không cần điều khiển chân DE.
- Đầu ra: `J4` (A/B, có cầu chì 0,1A và TVS), kèm đầu `J5` (UART TTL RX1/TX1).
- Đầu `CN2` (UART0: VIN, TX0_BOOT, RX0) để nạp qua cáp UART ngoài.

## 9. Header mở rộng JP1 (26 chân, 2 hàng)

📄 (trang 2). Các chân **tự do, chưa dùng**:

| Chân JP1 | Tín hiệu | Ghi chú |
|---|---|---|
| 1, 2 | 3V3, 5V | |
| 3 | GPIO1 | |
| 5 | GPIO2 | |
| 9 | GPIO3 | |
| 11 | GPIO4 | |
| 13 | GPIO5 | |
| 15 | GPIO20 | chung với `TOUCH_INT` |
| 17 | GPIO32 | |
| 19 | GPIO33 | |
| 10, 12, 14 | GPIO47, 46, 45 | GPIO45: điều khiển nguồn thẻ nhớ (R10 NC) |
| 21, 23 | I2C SDA/SCL | = GPIO7/8, **bus chung với codec** |
| 20, 22, 24, 26 | `C6_U0RXD`, `C6_U0TXD`, `C6_IO9`, `C6_CHIP_PU` | nạp/gỡ lỗi C6 |
| còn lại | GND, 3V3, 5V | |

Danh sách chân firmware cho phép gắn đèn LED ngoài (xem [README](../README.md)): **GPIO 1, 2, 3, 4, 5, 20, 32, 33, 46, 47**.

## 10. Bản đồ GPIO tổng hợp

| GPIO | Dùng cho | Ghi chú |
|---|---|---|
| 0 | (chưa ra header) | |
| 1–5 | header JP1 | tự do |
| 7, 8 | I2C (codec, cảm ứng, CN3) | ✅ |
| 9–13 | I2S ↔ ES8311 | ✅ |
| 14–19 | SDIO ↔ ESP32-C6 | ✅ |
| 20 | header / TOUCH_INT | |
| 21 | TOUCH_RST | |
| 22 | LCD_PWM | |
| 23 | (không rõ) | |
| 24, 25 | USB full-speed (cổng USB2) | ✅ |
| 26, 27 | RS485 (UART1) | |
| 28–31, 34, 35, 49–52 | Ethernet RMII | GPIO35 trùng nút BOOT |
| 32, 33 | header | tự do |
| 35 | BOOT, TXD1 Ethernet | ✅ nút |
| 36 | (kéo lên 3V3 qua R44) | tránh dùng |
| 37, 38 | UART0 ↔ CH340 | ✅ log |
| 39–44 | thẻ TF (SDMMC) | |
| 45 | nguồn thẻ TF (R10 NC) | |
| 46, 47 | header | tự do |
| 48 | ES7210_SDOUT | ❓ |
| 53 | PA_CTRL (loa) | ✅ |
| 54 | Reset ESP32-C6 | ✅ |

## 11. Điểm cần lưu ý khi viết firmware

1. **Chỉ có 1 mic analog, loa mono** trên cùng codec: nói và nghe cùng lúc cần AEC phần mềm.
2. **Wi-Fi qua C6** nên có độ trễ SDIO; cần giữ firmware C6 khớp với host (hiện lệch 2.3.0 ↔ 2.13).
3. **GPIO35 dùng chung** cho BOOT và Ethernet; nếu bật Ethernet thì đổi nút bấm.
4. **Không có LED người dùng**. Muốn có đèn trạng thái phải gắn ngoài vào header (khuyến nghị WS2812 ở GPIO20).
5. Bus I2C chung codec và cảm ứng: khi gắn màn hình, phải dùng chung `i2c_master_bus` (đừng khởi tạo hai bus trên cùng chân).
6. Chưa có bản schematic PDF gốc: sơ đồ ảnh chỉ là bản tham khảo, **kiểm tra bằng đồng hồ** trước khi nối thiết bị công suất lớn.
