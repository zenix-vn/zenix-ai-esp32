#include "settings.h"

#include <string.h>
#include "esp_log.h"
#include "nvs.h"

static const char *TAG = "settings";
static const char *NS = "cfg";

settings_t g_settings;

static void load_str(nvs_handle_t h, const char *key, char *dst, size_t cap, const char *def)
{
    size_t len = cap;
    if (h == 0 || nvs_get_str(h, key, dst, &len) != ESP_OK) {
        strlcpy(dst, def, cap);
    }
}

void settings_load(void)
{
    nvs_handle_t h = 0;
    if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK) {
        h = 0;
    }
    load_str(h, "ssid", g_settings.wifi_ssid, sizeof(g_settings.wifi_ssid), CONFIG_XZ_WIFI_SSID);
    load_str(h, "pass", g_settings.wifi_pass, sizeof(g_settings.wifi_pass), CONFIG_XZ_WIFI_PASSWORD);
    load_str(h, "ota_url", g_settings.ota_url, sizeof(g_settings.ota_url), CONFIG_XZ_OTA_URL);
    load_str(h, "ws_url", g_settings.ws_url, sizeof(g_settings.ws_url), CONFIG_XZ_WS_URL);
    load_str(h, "ws_token", g_settings.ws_token, sizeof(g_settings.ws_token), CONFIG_XZ_WS_TOKEN);
    int32_t vol = CONFIG_XZ_VOLUME, led_type = 0, led_gpio = -1, led_bri = 60;
    if (h) {
        nvs_get_i32(h, "volume", &vol);
        nvs_get_i32(h, "led_type", &led_type);
        nvs_get_i32(h, "led_gpio", &led_gpio);
        nvs_get_i32(h, "led_bri", &led_bri);
        nvs_close(h);
    }
    g_settings.volume = vol;
    g_settings.led_type = led_type;
    g_settings.led_gpio = led_gpio;
    g_settings.led_brightness = led_bri;
    ESP_LOGI(TAG, "Wi-Fi \"%s\", OTA %s", g_settings.wifi_ssid, g_settings.ota_url);
}

esp_err_t settings_save(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    nvs_set_str(h, "ssid", g_settings.wifi_ssid);
    nvs_set_str(h, "pass", g_settings.wifi_pass);
    nvs_set_str(h, "ota_url", g_settings.ota_url);
    nvs_set_str(h, "ws_url", g_settings.ws_url);
    nvs_set_str(h, "ws_token", g_settings.ws_token);
    nvs_set_i32(h, "volume", g_settings.volume);
    nvs_set_i32(h, "led_type", g_settings.led_type);
    nvs_set_i32(h, "led_gpio", g_settings.led_gpio);
    nvs_set_i32(h, "led_bri", g_settings.led_brightness);
    err = nvs_commit(h);
    nvs_close(h);
    return err;
}

esp_err_t settings_erase(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
    if (err == ESP_OK) {
        nvs_erase_all(h);
        err = nvs_commit(h);
        nvs_close(h);
    }
    return err;
}
