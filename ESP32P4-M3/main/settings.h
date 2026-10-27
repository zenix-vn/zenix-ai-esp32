#pragma once

#include "esp_err.h"

// Cau hinh thiet bi luu trong NVS (namespace "cfg"); gia tri mac dinh lay tu Kconfig.
typedef struct {
    char wifi_ssid[33];
    char wifi_pass[65];
    char ota_url[160];
    char ws_url[160];
    char ws_token[128];
    int volume;
    int led_type;        // led_type_t
    int led_gpio;
    int led_brightness;  // 5..100
} settings_t;

extern settings_t g_settings;

void settings_load(void);
esp_err_t settings_save(void);
// Xoa toan bo cau hinh (Wi-Fi, server) — dung cho "reset ve mac dinh".
esp_err_t settings_erase(void);
