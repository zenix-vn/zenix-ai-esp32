#include "web_server.h"

#include <string.h>
#include "audio.h"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "net_wifi.h"
#include "settings.h"
#include "status_led.h"

static const char *TAG = "web";

extern const char index_html_start[] asm("_binary_index_html_start");
extern const char index_html_end[] asm("_binary_index_html_end");

static esp_err_t send_json(httpd_req_t *req, cJSON *root)
{
    char *s = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    esp_err_t err = httpd_resp_sendstr(req, s);
    cJSON_free(s);
    return err;
}

static esp_err_t send_ok(httpd_req_t *req, bool ok, const char *message)
{
    cJSON *r = cJSON_CreateObject();
    cJSON_AddBoolToObject(r, "ok", ok);
    if (message) {
        cJSON_AddStringToObject(r, "message", message);
    }
    return send_json(req, r);
}

// Doc body JSON cua request (toi da 1 KB).
static cJSON *read_json(httpd_req_t *req)
{
    if (req->content_len <= 0 || req->content_len > 1024) {
        return NULL;
    }
    char buf[1025];
    int got = 0;
    while (got < req->content_len) {
        int n = httpd_req_recv(req, buf + got, req->content_len - got);
        if (n <= 0) {
            return NULL;
        }
        got += n;
    }
    buf[got] = '\0';
    return cJSON_Parse(buf);
}

static void copy_str(const cJSON *obj, const char *key, char *dst, size_t cap)
{
    const cJSON *it = cJSON_GetObjectItem(obj, key);
    if (cJSON_IsString(it)) {
        strlcpy(dst, it->valuestring, cap);
    }
}

static void restart_later(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(1500));
    esp_restart();
}

static esp_err_t index_get(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, index_html_start, index_html_end - index_html_start);
}

static esp_err_t status_get(httpd_req_t *req)
{
    char mac[18], ip[16];
    wifi_get_mac_str(mac);
    wifi_get_ip_str(ip);
    const esp_app_desc_t *app = esp_app_get_description();

    cJSON *r = cJSON_CreateObject();
    cJSON_AddBoolToObject(r, "setup_mode", wifi_is_setup_mode());
    cJSON_AddStringToObject(r, "mac", mac);
    cJSON_AddStringToObject(r, "ip", ip);
    cJSON_AddStringToObject(r, "wifi_ssid", g_settings.wifi_ssid);
    cJSON_AddNumberToObject(r, "rssi", wifi_get_rssi());
    cJSON_AddStringToObject(r, "firmware", app->version);
    cJSON_AddStringToObject(r, "idf", app->idf_ver);
    cJSON_AddNumberToObject(r, "uptime_s", (double)(esp_log_timestamp() / 1000));
    cJSON_AddNumberToObject(r, "free_heap", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    cJSON_AddNumberToObject(r, "free_psram", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    cJSON_AddNumberToObject(r, "volume", audio_get_volume());
    cJSON_AddStringToObject(r, "led_mode", status_led_mode_name(status_led_mode()));
    cJSON_AddStringToObject(r, "ota_url", g_settings.ota_url);
    cJSON_AddStringToObject(r, "ws_url", g_settings.ws_url);
    cJSON_AddStringToObject(r, "ws_token", g_settings.ws_token);
    app_fill_status(r);
    return send_json(req, r);
}

static esp_err_t scan_get(httpd_req_t *req)
{
    cJSON *r = cJSON_CreateObject();
    cJSON_AddItemToObject(r, "networks", wifi_scan());
    return send_json(req, r);
}

static esp_err_t wifi_post(httpd_req_t *req)
{
    cJSON *body = read_json(req);
    const cJSON *ssid = cJSON_GetObjectItem(body, "ssid");
    if (!cJSON_IsString(ssid) || ssid->valuestring[0] == '\0') {
        cJSON_Delete(body);
        return send_ok(req, false, "Thiếu tên Wi-Fi");
    }
    char new_ssid[33], new_pass[65] = "";
    strlcpy(new_ssid, ssid->valuestring, sizeof(new_ssid));
    copy_str(body, "password", new_pass, sizeof(new_pass));
    cJSON_Delete(body);

    // Thu ket noi truoc khi luu de bao loi sai mat khau ngay tren trang.
    if (!wifi_sta_connect(new_ssid, new_pass, 20000)) {
        return send_ok(req, false, "Không kết nối được. Kiểm tra mật khẩu hoặc vùng phủ sóng.");
    }
    strlcpy(g_settings.wifi_ssid, new_ssid, sizeof(g_settings.wifi_ssid));
    strlcpy(g_settings.wifi_pass, new_pass, sizeof(g_settings.wifi_pass));
    settings_save();

    char ip[16];
    wifi_get_ip_str(ip);
    cJSON *r = cJSON_CreateObject();
    cJSON_AddBoolToObject(r, "ok", true);
    cJSON_AddStringToObject(r, "ip", ip);
    cJSON_AddStringToObject(r, "message", "Đã kết nối. Thiết bị sẽ khởi động lại.");
    send_json(req, r);
    xTaskCreate(restart_later, "restart", 2048, NULL, 5, NULL);
    return ESP_OK;
}

static esp_err_t config_post(httpd_req_t *req)
{
    cJSON *body = read_json(req);
    if (!body) {
        return send_ok(req, false, "Dữ liệu không hợp lệ");
    }
    copy_str(body, "ota_url", g_settings.ota_url, sizeof(g_settings.ota_url));
    copy_str(body, "ws_url", g_settings.ws_url, sizeof(g_settings.ws_url));
    copy_str(body, "ws_token", g_settings.ws_token, sizeof(g_settings.ws_token));
    const cJSON *vol = cJSON_GetObjectItem(body, "volume");
    if (cJSON_IsNumber(vol)) {
        audio_set_volume(vol->valueint);
        g_settings.volume = audio_get_volume();
    }
    bool server_changed = cJSON_HasObjectItem(body, "ota_url") || cJSON_HasObjectItem(body, "ws_url");
    cJSON_Delete(body);
    settings_save();
    return send_ok(req, true, server_changed ? "Đã lưu. Khởi động lại để áp dụng server mới." : "Đã lưu.");
}

static esp_err_t led_get(httpd_req_t *req)
{
    cJSON *r = cJSON_CreateObject();
    cJSON_AddNumberToObject(r, "type", g_settings.led_type);
    cJSON_AddNumberToObject(r, "gpio", g_settings.led_gpio);
    cJSON_AddNumberToObject(r, "brightness", g_settings.led_brightness);
    cJSON_AddStringToObject(r, "mode", status_led_mode_name(status_led_mode()));
    cJSON *gpios = cJSON_AddArrayToObject(r, "safe_gpios");
    for (const int *g = STATUS_LED_SAFE_GPIOS; *g >= 0; g++) {
        cJSON_AddItemToArray(gpios, cJSON_CreateNumber(*g));
    }
    return send_json(req, r);
}

// Ap dung ngay (de thu tung chan) va luu vao NVS.
static esp_err_t led_post(httpd_req_t *req)
{
    cJSON *body = read_json(req);
    if (!body) {
        return send_ok(req, false, "Dữ liệu không hợp lệ");
    }
    const cJSON *type = cJSON_GetObjectItem(body, "type");
    const cJSON *gpio = cJSON_GetObjectItem(body, "gpio");
    const cJSON *bri = cJSON_GetObjectItem(body, "brightness");
    int new_type = cJSON_IsNumber(type) ? type->valueint : g_settings.led_type;
    int new_gpio = cJSON_IsNumber(gpio) ? gpio->valueint : g_settings.led_gpio;
    int new_bri = cJSON_IsNumber(bri) ? bri->valueint : g_settings.led_brightness;
    bool test = cJSON_IsTrue(cJSON_GetObjectItem(body, "test"));
    cJSON_Delete(body);
    if (new_type != LED_TYPE_NONE && !status_led_gpio_allowed(new_gpio)) {
        return send_ok(req, false, "Chân GPIO không an toàn để gắn LED");
    }
    g_settings.led_type = new_type;
    g_settings.led_gpio = new_gpio;
    g_settings.led_brightness = new_bri;
    status_led_configure(g_settings.led_type, g_settings.led_gpio, g_settings.led_brightness);
    settings_save();
    if (test) {
        status_led_overlay(LED_TEST, 3000);
    }
    return send_ok(req, true, test ? "Đèn sẽ nháy trắng trong 3 giây." : "Đã lưu.");
}

static esp_err_t reboot_post(httpd_req_t *req)
{
    send_ok(req, true, "Đang khởi động lại...");
    xTaskCreate(restart_later, "restart", 2048, NULL, 5, NULL);
    return ESP_OK;
}

static esp_err_t reset_post(httpd_req_t *req)
{
    settings_erase();
    send_ok(req, true, "Đã xoá cấu hình. Thiết bị sẽ phát lại Wi-Fi " SETUP_AP_SSID ".");
    xTaskCreate(restart_later, "restart", 2048, NULL, 5, NULL);
    return ESP_OK;
}

// Moi URL la (vd. kiem tra captive portal cua dien thoai) -> chuyen ve trang chu.
static esp_err_t redirect_404(httpd_req_t *req, httpd_err_code_t err)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", wifi_is_setup_mode() ? "http://" SETUP_AP_IP "/" : "/");
    return httpd_resp_send(req, NULL, 0);
}

void web_server_start(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.stack_size = 8192;
    cfg.max_uri_handlers = 12;
    cfg.lru_purge_enable = true;
    cfg.max_open_sockets = 7;
    httpd_handle_t server;
    if (httpd_start(&server, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed");
        return;
    }
    const httpd_uri_t uris[] = {
        {.uri = "/", .method = HTTP_GET, .handler = index_get},
        {.uri = "/api/status", .method = HTTP_GET, .handler = status_get},
        {.uri = "/api/scan", .method = HTTP_GET, .handler = scan_get},
        {.uri = "/api/wifi", .method = HTTP_POST, .handler = wifi_post},
        {.uri = "/api/config", .method = HTTP_POST, .handler = config_post},
        {.uri = "/api/reboot", .method = HTTP_POST, .handler = reboot_post},
        {.uri = "/api/led", .method = HTTP_GET, .handler = led_get},
        {.uri = "/api/led", .method = HTTP_POST, .handler = led_post},
        {.uri = "/api/reset", .method = HTTP_POST, .handler = reset_post},
    };
    for (size_t i = 0; i < sizeof(uris) / sizeof(uris[0]); i++) {
        httpd_register_uri_handler(server, &uris[i]);
    }
    httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, redirect_404);
    ESP_LOGI(TAG, "Web quan ly dang chay tren cong 80");
}
