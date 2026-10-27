// Zenix AI client cho ESP32-P4: giu nut BOOT de noi chuyen voi server AI theo giao thuc xiaozhi.
#include <stdio.h>
#include <string.h>
#include "audio.h"
#include "board.h"
#include "captive_dns.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "mcp.h"
#include "esp_netif.h"
#include "mdns.h"
#include "net_wifi.h"
#include "nvs_flash.h"
#include "ota_activate.h"
#include "settings.h"
#include "status_led.h"
#include "web_server.h"
#include "xz_protocol.h"

static const char *TAG = "app";

typedef enum { ST_IDLE, ST_CONNECTING, ST_LISTENING, ST_SPEAKING } app_state_t;
static const char *const STATE_NAMES[] = {"idle", "connecting", "listening", "speaking"};

typedef struct {
    size_t len;
    uint8_t data[];
} opus_packet_t;

static volatile app_state_t s_state = ST_IDLE;
// Giai doan truoc vong hoi thoai (hien tren trang quan ly); NULL = dung s_state.
static const char *volatile s_phase = "starting";
static QueueHandle_t s_play_q;
static SemaphoreHandle_t s_conn_lock;
static xz_server_cfg_t s_cfg;

static void set_state(app_state_t st)
{
    if (s_state != st) {
        ESP_LOGI(TAG, "State: %s -> %s", STATE_NAMES[s_state], STATE_NAMES[st]);
        s_state = st;
    }
    static const led_mode_t LED_FOR_STATE[] = {
        [ST_IDLE] = LED_IDLE, [ST_CONNECTING] = LED_SERVER, [ST_LISTENING] = LED_LISTENING, [ST_SPEAKING] = LED_SPEAKING,
    };
    status_led_set(LED_FOR_STATE[st]);
    if (st != ST_IDLE) {
        status_led_overlay(LED_IDLE, 0);  // bo "dang nghi" khi da co trang thai moi
    }
}

static void flush_playback(void)
{
    opus_packet_t *p;
    while (xQueueReceive(s_play_q, &p, 0) == pdTRUE) {
        free(p);
    }
    audio_reset_decoder();
}

static void send_simple(const char *type, const char *state)
{
    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "type", type);
    if (state) {
        cJSON_AddStringToObject(msg, "state", state);
        cJSON_AddStringToObject(msg, "mode", "manual");
    }
    xz_send_json(msg);
    cJSON_Delete(msg);
}

// ---- Callback tu task websocket ----

static void on_json(const cJSON *root)
{
    const cJSON *type = cJSON_GetObjectItem(root, "type");
    if (!cJSON_IsString(type)) {
        return;
    }
    const char *t = type->valuestring;
    const cJSON *text = cJSON_GetObjectItem(root, "text");
    const char *txt = cJSON_IsString(text) ? text->valuestring : "";

    if (strcmp(t, "tts") == 0) {
        const cJSON *st = cJSON_GetObjectItem(root, "state");
        const char *s = cJSON_IsString(st) ? st->valuestring : "";
        if (strcmp(s, "start") == 0 && s_state != ST_LISTENING) {
            set_state(ST_SPEAKING);
        } else if (strcmp(s, "stop") == 0 && s_state == ST_SPEAKING) {
            set_state(ST_IDLE);
        } else if (strcmp(s, "sentence_start") == 0) {
            ESP_LOGI(TAG, "AI  >> %s", txt);
        }
    } else if (strcmp(t, "stt") == 0) {
        ESP_LOGI(TAG, "Ban >> %s", txt);
    } else if (strcmp(t, "llm") == 0) {
        const cJSON *emo = cJSON_GetObjectItem(root, "emotion");
        ESP_LOGI(TAG, "Cam xuc: %s %s", cJSON_IsString(emo) ? emo->valuestring : "", txt);
    } else if (strcmp(t, "mcp") == 0) {
        mcp_handle(cJSON_GetObjectItem(root, "payload"));
    } else if (strcmp(t, "system") == 0) {
        const cJSON *cmd = cJSON_GetObjectItem(root, "command");
        if (cJSON_IsString(cmd) && strcmp(cmd->valuestring, "reboot") == 0) {
            esp_restart();
        }
    } else if (strcmp(t, "alert") == 0) {
        const cJSON *m = cJSON_GetObjectItem(root, "message");
        ESP_LOGW(TAG, "Alert: %s", cJSON_IsString(m) ? m->valuestring : "");
        status_led_overlay(LED_ERROR, 3000);
    }
}

static void on_audio(const uint8_t *data, size_t len)
{
    if (s_state == ST_LISTENING) {
        return;  // dang thu am: bo audio cu cua server
    }
    opus_packet_t *p = malloc(sizeof(opus_packet_t) + len);
    if (!p) {
        return;
    }
    p->len = len;
    memcpy(p->data, data, len);
    // Chan task websocket mot chut neu hang doi day: server gui nhanh hon toc do phat.
    if (xQueueSend(s_play_q, &p, pdMS_TO_TICKS(2000)) != pdTRUE) {
        free(p);
    }
}

static void on_closed(void)
{
    set_state(ST_IDLE);
    status_led_set(LED_OFFLINE);  // task giu ket noi se thu lai
}

void app_fill_status(cJSON *root)
{
    cJSON_AddStringToObject(root, "app_state", s_phase ? s_phase : STATE_NAMES[s_state]);
    cJSON_AddStringToObject(root, "activation_code", xz_ota_activation_code());
    cJSON_AddStringToObject(root, "activation_message", xz_ota_activation_message());
}

// ---- Task am thanh ----

static void mic_task(void *arg)
{
    static uint8_t opus[1500];
    while (true) {
        if (s_state != ST_LISTENING) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        int n = audio_record_opus(opus, sizeof(opus));
        if (n > 0 && s_state == ST_LISTENING) {
            xz_send_audio(opus, n);
        }
    }
}

static void play_task(void *arg)
{
    opus_packet_t *p;
    while (true) {
        if (xQueueReceive(s_play_q, &p, portMAX_DELAY) == pdTRUE) {
            audio_play_opus(p->data, p->len);
            free(p);
        }
    }
}

// ---- Nut nhan: giu de noi, nha de gui ----

// Mo ket noi neu chua co. Duoc goi tu task giu ket noi va tu nut nhan, nen can khoa.
static bool ensure_connected(void)
{
    xSemaphoreTake(s_conn_lock, portMAX_DELAY);
    bool ok = xz_is_open() || xz_open();
    xSemaphoreGive(s_conn_lock);
    return ok;
}

// Giu ket noi WebSocket thuong truc: server thay thiet bi "truc tuyen", lan noi dau
// khong phai cho bat tay, va server biet san cac tool MCP cua thiet bi.
static void conn_task(void *arg)
{
    int backoff_s = 2;
    while (true) {
        if (xz_is_open()) {
            backoff_s = 2;
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        if (status_led_mode() != LED_OFFLINE) {
            status_led_set(LED_SERVER);
        }
        if (ensure_connected()) {
            ESP_LOGI(TAG, "Da ket noi server");
            status_led_set(LED_IDLE);
            continue;
        }
        status_led_set(LED_OFFLINE);
        ESP_LOGW(TAG, "Ket noi server that bai, thu lai sau %ds", backoff_s);
        vTaskDelay(pdMS_TO_TICKS(backoff_s * 1000));
        backoff_s = backoff_s < 60 ? backoff_s * 2 : 60;
    }
}

static void on_button_down(void)
{
    if (s_state == ST_SPEAKING || uxQueueMessagesWaiting(s_play_q) > 0) {
        send_simple("abort", NULL);  // ngat AI dang noi
        flush_playback();
    }
    if (!xz_is_open()) {
        set_state(ST_CONNECTING);
        if (!ensure_connected()) {
            set_state(ST_IDLE);
            return;
        }
    }
    send_simple("listen", "start");
    set_state(ST_LISTENING);
}

static void on_button_up(void)
{
    if (s_state == ST_LISTENING) {
        set_state(ST_IDLE);
        send_simple("listen", "stop");
        status_led_overlay(LED_THINKING, 15000);  // toi khi AI bat dau noi (hoac het 15 s)
    }
}

// mDNS: ten chinh "ai-setup.local" (de nho, dung ca o che do cai dat lan mang nha),
// kem ten phu theo MAC "zin-xxxx.local" de phan biet khi co nhieu thiet bi.
static void start_mdns(bool setup_mode)
{
    if (mdns_init() != ESP_OK) {
        ESP_LOGE(TAG, "mDNS init failed");
        return;
    }
    mdns_hostname_set(SETUP_HOSTNAME);
    mdns_instance_name_set("Zin AI device");
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);

    if (!setup_mode) {
        char mac[18], alias[16];
        wifi_get_mac_str(mac);
        snprintf(alias, sizeof(alias), "zin-%c%c%c%c", mac[12], mac[13], mac[15], mac[16]);
        esp_netif_ip_info_t ip;
        if (esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip) == ESP_OK) {
            mdns_ip_addr_t addr = {.addr = {.u_addr.ip4 = ip.ip, .type = ESP_IPADDR_TYPE_V4}};
            mdns_delegate_hostname_add(alias, &addr);
        }
        ESP_LOGI(TAG, "Trang quan ly: http://%s.local (hoac http://%s.local)", SETUP_HOSTNAME, alias);
    } else {
        ESP_LOGI(TAG, "Trang cai dat: http://%s.local hoac http://%s", SETUP_HOSTNAME, SETUP_AP_IP);
    }
}

// Che do cai dat: phat Wi-Fi Zin-Setup, DNS bat moi ten mien, cho nguoi dung chon Wi-Fi nha.
// Trang web se luu cau hinh va khoi dong lai thiet bi.
static void run_setup_portal(void)
{
    s_phase = "offline";
    status_led_set(LED_SETUP);
    wifi_start_setup_ap();
    captive_dns_start();
    web_server_start();
    start_mdns(true);
    while (true) {
        vTaskDelay(portMAX_DELAY);
    }
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    settings_load();
    status_led_init();
    status_led_configure(g_settings.led_type, g_settings.led_gpio, g_settings.led_brightness);
    status_led_set(LED_BOOT);

    i2c_master_bus_handle_t bus = board_i2c_init();
    if (!board_i2c_has_device(CONFIG_XZ_ES8311_ADDR)) {
        ESP_LOGE(TAG, "Khong thay ES8311 o 0x%02x — xem ket qua quet I2C o tren va sua chan trong menuconfig",
                 CONFIG_XZ_ES8311_ADDR);
    }
    if (audio_init(bus) != ESP_OK) {
        ESP_LOGE(TAG, "Audio init failed, dung lai.");
        return;
    }
    audio_set_volume(g_settings.volume);
    board_button_init();

    // Giu nut BOOT luc khoi dong de vao che do cai dat.
    bool force_setup = board_button_pressed();
    wifi_init();
    status_led_set(LED_WIFI);
    if (force_setup || !wifi_sta_connect(g_settings.wifi_ssid, g_settings.wifi_pass, 30000)) {
        ESP_LOGW(TAG, "%s: vao che do cai dat",
                 force_setup ? "Nut BOOT duoc giu" : "Chua co Wi-Fi hoac khong ket noi duoc");
        run_setup_portal();
    }

    web_server_start();
    start_mdns(false);

    s_phase = "activating";
    status_led_set(LED_SERVER);
    if (xz_ota_activate(&s_cfg) != ESP_OK) {
        s_phase = "error";
        status_led_set(LED_ERROR);
        ESP_LOGE(TAG, "Khong lay duoc cau hinh server, khoi dong lai sau 60s (trang quan ly van mo)");
        vTaskDelay(pdMS_TO_TICKS(60000));
        esp_restart();
    }

    s_play_q = xQueueCreate(64, sizeof(opus_packet_t *));
    xz_callbacks_t cb = {.on_json = on_json, .on_audio = on_audio, .on_closed = on_closed};
    xz_protocol_init(&s_cfg, &cb);

    // Opus encoder/decoder can stack lon.
    xTaskCreate(mic_task, "mic", 24 * 1024, NULL, 6, NULL);
    xTaskCreate(play_task, "play", 24 * 1024, NULL, 5, NULL);
    s_conn_lock = xSemaphoreCreateMutex();
    xTaskCreate(conn_task, "conn", 6 * 1024, NULL, 4, NULL);

    s_phase = NULL;
    ESP_LOGI(TAG, "San sang! Giu nut BOOT (GPIO%d) de noi, nha ra de gui.", CONFIG_XZ_BUTTON_GPIO);

    bool was_pressed = false;
    while (true) {
        bool pressed = board_button_pressed();
        if (pressed != was_pressed) {
            vTaskDelay(pdMS_TO_TICKS(30));  // chong doi
            pressed = board_button_pressed();
        }
        if (pressed && !was_pressed) {
            on_button_down();
        } else if (!pressed && was_pressed) {
            on_button_up();
        }
        was_pressed = pressed;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
