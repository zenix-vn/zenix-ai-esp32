#include "app.h"
#include "nvs.h"

settings_t g_cfg = {.h12 = 0, .auto_home = 1, .rot = 0};

static const int AUTO_S[] = {30, 60, 300, 0};
static const char *AUTO_TXT[] = {"30 giây", "1 phút", "5 phút", "Tắt"};

int settings_auto_home_s(void) { return AUTO_S[g_cfg.auto_home % 4]; }
const char *settings_auto_home_text(void) { return AUTO_TXT[g_cfg.auto_home % 4]; }

void settings_load(void) {
    nvs_handle_t h;
    if (nvs_open("cfg", NVS_READONLY, &h) != ESP_OK) return;
    settings_t t;
    size_t n = sizeof t;
    if (nvs_get_blob(h, "s", &t, &n) == ESP_OK && n == sizeof t) g_cfg = t;
    nvs_close(h);
}

void settings_save(void) {
    nvs_handle_t h;
    if (nvs_open("cfg", NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_blob(h, "s", &g_cfg, sizeof g_cfg);
    nvs_commit(h);
    nvs_close(h);
}
