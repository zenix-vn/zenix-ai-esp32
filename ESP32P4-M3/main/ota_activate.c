#include "ota_activate.h"

#include <stdio.h>
#include <string.h>
#include "cJSON.h"
#include "esp_app_desc.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "net_wifi.h"
#include "nvs.h"
#include "settings.h"
#include "status_led.h"

static const char *TAG = "ota";

#define RESP_MAX 8192

static char s_act_code[16];
static char s_act_message[128];

const char *xz_ota_activation_code(void)
{
    return s_act_code;
}

const char *xz_ota_activation_message(void)
{
    return s_act_message;
}

static void load_client_id(char out[37])
{
    nvs_handle_t nvs;
    size_t len = 37;
    ESP_ERROR_CHECK(nvs_open("xz", NVS_READWRITE, &nvs));
    if (nvs_get_str(nvs, "uuid", out, &len) != ESP_OK) {
        uint8_t b[16];
        esp_fill_random(b, sizeof(b));
        b[6] = (b[6] & 0x0f) | 0x40;  // UUID v4
        b[8] = (b[8] & 0x3f) | 0x80;
        snprintf(out, 37, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                 b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7], b[8], b[9], b[10], b[11], b[12], b[13], b[14], b[15]);
        nvs_set_str(nvs, "uuid", out);
        nvs_commit(nvs);
    }
    nvs_close(nvs);
}

static char *build_system_info(const xz_server_cfg_t *cfg)
{
    const esp_app_desc_t *app = esp_app_get_description();
    char sha[65];
    for (int i = 0; i < 32; i++) {
        snprintf(sha + i * 2, 3, "%02x", app->app_elf_sha256[i]);
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "version", 2);
    cJSON_AddStringToObject(root, "language", "vi-VN");
    cJSON_AddStringToObject(root, "mac_address", cfg->device_id);
    cJSON_AddStringToObject(root, "uuid", cfg->client_id);
    cJSON_AddStringToObject(root, "chip_model_name", CONFIG_IDF_TARGET);
    cJSON *a = cJSON_AddObjectToObject(root, "application");
    cJSON_AddStringToObject(a, "name", app->project_name);
    cJSON_AddStringToObject(a, "version", app->version);
    cJSON_AddStringToObject(a, "idf_version", app->idf_ver);
    cJSON_AddStringToObject(a, "elf_sha256", sha);
    cJSON *board = cJSON_AddObjectToObject(root, "board");
    cJSON_AddStringToObject(board, "type", CONFIG_XZ_BOARD_NAME);
    cJSON_AddStringToObject(board, "name", CONFIG_XZ_BOARD_NAME);
    cJSON_AddStringToObject(board, "mac", cfg->device_id);
    char *s = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return s;
}

// POST body len url; tra ve HTTP status, phan hoi ghi vao resp (co the NULL).
static int http_post(const xz_server_cfg_t *cfg, const char *url, const char *body, char *resp, int resp_cap)
{
    esp_http_client_config_t hc = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 10000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t c = esp_http_client_init(&hc);
    char ua[64];
    snprintf(ua, sizeof(ua), "%s/%s", CONFIG_XZ_BOARD_NAME, esp_app_get_description()->version);
    esp_http_client_set_header(c, "Activation-Version", "1");
    esp_http_client_set_header(c, "Device-Id", cfg->device_id);
    esp_http_client_set_header(c, "Client-Id", cfg->client_id);
    esp_http_client_set_header(c, "User-Agent", ua);
    esp_http_client_set_header(c, "Accept-Language", "vi-VN");
    esp_http_client_set_header(c, "Content-Type", "application/json");

    int status = -1;
    int body_len = strlen(body);
    if (esp_http_client_open(c, body_len) == ESP_OK && esp_http_client_write(c, body, body_len) == body_len) {
        esp_http_client_fetch_headers(c);
        status = esp_http_client_get_status_code(c);
        if (resp) {
            int total = 0, n;
            while (total < resp_cap - 1 && (n = esp_http_client_read(c, resp + total, resp_cap - 1 - total)) > 0) {
                total += n;
            }
            resp[total] = '\0';
        }
    } else {
        ESP_LOGE(TAG, "HTTP request to %s failed", url);
    }
    esp_http_client_cleanup(c);
    return status;
}

static void copy_json_str(cJSON *obj, const char *key, char *dst, size_t cap)
{
    cJSON *it = cJSON_GetObjectItem(obj, key);
    if (cJSON_IsString(it)) {
        strlcpy(dst, it->valuestring, cap);
    }
}

esp_err_t xz_ota_activate(xz_server_cfg_t *cfg)
{
    wifi_get_mac_str(cfg->device_id);
    load_client_id(cfg->client_id);
    ESP_LOGI(TAG, "Device-Id %s, Client-Id %s", cfg->device_id, cfg->client_id);

    if (g_settings.ws_url[0] != '\0') {
        strlcpy(cfg->ws_url, g_settings.ws_url, sizeof(cfg->ws_url));
        strlcpy(cfg->ws_token, g_settings.ws_token, sizeof(cfg->ws_token));
        ESP_LOGI(TAG, "Dung WebSocket URL co dinh: %s", cfg->ws_url);
        return ESP_OK;
    }

    const char *ota_url = g_settings.ota_url;
    if (strlen(ota_url) < 10) {
        ESP_LOGE(TAG, "OTA URL chua duoc cau hinh");
        return ESP_FAIL;
    }
    char activate_url[192];
    snprintf(activate_url, sizeof(activate_url), "%s%sactivate", ota_url, ota_url[strlen(ota_url) - 1] == '/' ? "" : "/");

    char *resp = malloc(RESP_MAX);
    char *info = build_system_info(cfg);
    esp_err_t result = ESP_FAIL;

    for (int attempt = 0; attempt < 30; attempt++) {
        int status = http_post(cfg, ota_url, info, resp, RESP_MAX);
        if (status != 200) {
            ESP_LOGW(TAG, "OTA check failed (HTTP %d), thu lai sau 10s", status);
            vTaskDelay(pdMS_TO_TICKS(10000));
            continue;
        }

        cJSON *root = cJSON_Parse(resp);
        if (!root) {
            ESP_LOGE(TAG, "OTA response khong phai JSON: %.200s", resp);
            break;
        }
        cJSON *ws = cJSON_GetObjectItem(root, "websocket");
        if (cJSON_IsObject(ws)) {
            copy_json_str(ws, "url", cfg->ws_url, sizeof(cfg->ws_url));
            copy_json_str(ws, "token", cfg->ws_token, sizeof(cfg->ws_token));
        }
        cJSON *fw = cJSON_GetObjectItem(root, "firmware");
        if (cJSON_IsObject(fw)) {
            cJSON *v = cJSON_GetObjectItem(fw, "version");
            ESP_LOGI(TAG, "Server firmware version: %s (bo qua cap nhat)", cJSON_IsString(v) ? v->valuestring : "?");
        }

        cJSON *act = cJSON_GetObjectItem(root, "activation");
        if (!cJSON_IsObject(act)) {
            cJSON_Delete(root);
            s_act_code[0] = s_act_message[0] = '\0';
            if (cfg->ws_url[0] == '\0') {
                ESP_LOGE(TAG, "Server khong tra ve cau hinh websocket (chi co MQTT?)");
                break;
            }
            ESP_LOGI(TAG, "Da kich hoat. WebSocket: %s", cfg->ws_url);
            status_led_set(LED_SERVER);
            result = ESP_OK;
            break;
        }

        char *code = s_act_code, *message = s_act_message;
        copy_json_str(act, "code", s_act_code, sizeof(s_act_code));
        copy_json_str(act, "message", s_act_message, sizeof(s_act_message));
        bool has_challenge = cJSON_IsString(cJSON_GetObjectItem(act, "challenge"));
        cJSON_Delete(root);

        status_led_set(LED_ACTIVATING);
        ESP_LOGW(TAG, "==================================================");
        ESP_LOGW(TAG, "  MA KICH HOAT: %s", code);
        ESP_LOGW(TAG, "  %s", message);
        ESP_LOGW(TAG, "  Nhap ma tren trang quan tri server (vd. https://dev.zenix.vn -> Them thiet bi)");
        ESP_LOGW(TAG, "==================================================");

        if (!has_challenge) {
            // Khong co challenge: chi can hoi lai OTA cho toi khi het muc "activation".
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }
        // Hoi /activate cho den khi nguoi dung nhap ma (202 = dang cho).
        for (int i = 0; i < 60; i++) {
            int st = http_post(cfg, activate_url, "{}", NULL, 0);
            if (st == 200) {
                ESP_LOGI(TAG, "Kich hoat thanh cong");
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(st == 202 ? 3000 : 10000));
        }
    }

    free(info);
    free(resp);
    return result;
}
