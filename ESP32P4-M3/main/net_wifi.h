#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "cJSON.h"

#define SETUP_AP_SSID "Zin-Setup"
#define SETUP_AP_IP   "192.168.4.1"
#define SETUP_HOSTNAME "ai-setup"  // -> http://ai-setup.local

// Khoi tao Wi-Fi (qua ESP32-C6 bang esp_wifi_remote). Goi mot lan khi khoi dong.
void wifi_init(void);
// Ket noi STA; chan toi da timeout_ms. Neu that bai thi ngung thu lai (de con quet duoc).
bool wifi_sta_connect(const char *ssid, const char *pass, uint32_t timeout_ms);
// Bat AP "Zin-Setup" (mo, khong mat khau) song song voi STA.
void wifi_start_setup_ap(void);
bool wifi_is_setup_mode(void);

// Quet mang xung quanh; tra ve mang JSON [{ssid, rssi, secure, channel}] (nguoi goi giai phong).
cJSON *wifi_scan(void);

void wifi_get_mac_str(char out[18]);
void wifi_get_ip_str(char out[16]);
int wifi_get_rssi(void);
