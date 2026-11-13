# Zenix Desk Clock (ESP32-C3 + 2.4" ST7789V)

Desk clock firmware for a Luat Core-ESP32 (ESP32-C3) board on a 2.4" 320x240 ST7789V SPI display.

## Features
- **Home**: large clock, solar + lunar date, current weather, Wi-Fi signal.
- **Apps**: Weather (now / 24 h / 7 days, UV, AQI, sunrise/sunset), Calendar (Vietnamese lunar calendar,
  day and month views), Clock (analog, stopwatch, timer), Settings (12/24 h, auto-home, rotation, Wi-Fi reset, device info).
- One-button navigation with BOOT: **press** = next, **hold ~0.6 s** = select, **hold ~1.8 s** = back.
- Wi-Fi setup portal: join `Zenix-Clock`, open `http://192.168.4.1`, pick a network from the scan list.
- Weather from [Open-Meteo](https://open-meteo.com) (no API key); location and time zone from ip-api.com.

## Wiring
| Signal | GPIO |
|---|---|
| SCK | 2 |
| MOSI | 3 (10 on the board with MAC `10:20:ba:ce:4f:a4`, see `pick_mosi()` in `main/lcd.c`) |
| CS | 7 |
| DC | 4 |
| RST | 5 |
| Backlight | always on (R2 from 3.3 V) |

The panel needs colour inversion (INVON) even though it is sold as TN.

## Build
ESP-IDF v6.0, target `esp32c3`. LVGL 9.5 is pulled by the component manager.

```sh
idf.py set-target esp32c3
idf.py build
idf.py -p /dev/cu.usbmodemXXXX -b 460800 flash
```

## Notes
- Never use GPIO18/19: they are the native USB pins.
- On weak USB power, Wi-Fi start can brown out the display. TX power is capped at 11 dBm and the panel
  is re-initialised every 30 s.
