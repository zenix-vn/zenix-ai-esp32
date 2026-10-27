#include "net_wifi.h"

#include <stdio.h>
#include <string.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

static const char *TAG = "wifi";

#define GOT_IP_BIT BIT0
#define FAIL_BIT   BIT1
#define MAX_RETRY  5

static EventGroupHandle_t s_events;
static esp_netif_t *s_sta_netif;
static bool s_setup_mode;
static bool s_connecting;  // dang trong wifi_sta_connect()
static bool s_connected_once;
static int s_retry;

static void on_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *e = data;
        xEventGroupClearBits(s_events, GOT_IP_BIT);
        // Dang thu ket noi: thu toi MAX_RETRY lan. Da tung ket noi: luon thu lai (mat song tam thoi).
        if ((s_connecting && ++s_retry < MAX_RETRY) || (!s_connecting && s_connected_once)) {
            ESP_LOGW(TAG, "Disconnected (reason %d), retrying", e->reason);
            esp_wifi_connect();
        } else if (s_connecting) {
            ESP_LOGW(TAG, "Connect failed (reason %d)", e->reason);
            xEventGroupSetBits(s_events, FAIL_BIT);
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = data;
        ESP_LOGI(TAG, "Got IP " IPSTR, IP2STR(&e->ip_info.ip));
        s_connected_once = true;
        xEventGroupSetBits(s_events, GOT_IP_BIT);
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_AP_STACONNECTED) {
        ESP_LOGI(TAG, "Client joined %s", SETUP_AP_SSID);
    }
}

void wifi_init(void)
{
    s_events = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    s_sta_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));  // cau hinh do settings.c quan ly
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_event, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

bool wifi_sta_connect(const char *ssid, const char *pass, uint32_t timeout_ms)
{
    if (ssid == NULL || ssid[0] == '\0') {
        return false;
    }
    esp_wifi_disconnect();
    wifi_config_t cfg = {0};
    strlcpy((char *)cfg.sta.ssid, ssid, sizeof(cfg.sta.ssid));
    strlcpy((char *)cfg.sta.password, pass ? pass : "", sizeof(cfg.sta.password));
    cfg.sta.threshold.authmode = (pass && pass[0]) ? WIFI_AUTH_WEP : WIFI_AUTH_OPEN;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &cfg));

    ESP_LOGI(TAG, "Connecting to \"%s\"...", ssid);
    s_retry = 0;
    s_connecting = true;
    xEventGroupClearBits(s_events, GOT_IP_BIT | FAIL_BIT);
    esp_wifi_connect();
    EventBits_t bits = xEventGroupWaitBits(s_events, GOT_IP_BIT | FAIL_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(timeout_ms));
    s_connecting = false;
    if (!(bits & GOT_IP_BIT)) {
        esp_wifi_disconnect();
        return false;
    }
    return true;
}

void wifi_start_setup_ap(void)
{
    esp_netif_create_default_wifi_ap();
    wifi_config_t ap = {
        .ap = {
            .ssid = SETUP_AP_SSID,
            .ssid_len = strlen(SETUP_AP_SSID),
            .channel = 1,
            .authmode = WIFI_AUTH_OPEN,
            .max_connection = 4,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));
    s_setup_mode = true;
    ESP_LOGW(TAG, "Setup AP \"%s\" dang phat — ket noi roi mo http://%s", SETUP_AP_SSID, SETUP_AP_IP);
}

bool wifi_is_setup_mode(void)
{
    return s_setup_mode;
}

cJSON *wifi_scan(void)
{
    cJSON *arr = cJSON_CreateArray();
    wifi_scan_config_t sc = {.show_hidden = false};
    if (esp_wifi_scan_start(&sc, true) != ESP_OK) {
        ESP_LOGW(TAG, "Scan failed");
        return arr;
    }
    uint16_t n = 30;
    wifi_ap_record_t *recs = calloc(n, sizeof(wifi_ap_record_t));
    if (recs && esp_wifi_scan_get_ap_records(&n, recs) == ESP_OK) {
        for (int i = 0; i < n; i++) {
            const char *ssid = (const char *)recs[i].ssid;
            if (ssid[0] == '\0') {
                continue;
            }
            // Bo trung ten (nhieu AP cung SSID): giu ban ghi dau tien (manh nhat).
            bool dup = false;
            for (int j = 0; j < i; j++) {
                if (strcmp(ssid, (const char *)recs[j].ssid) == 0) {
                    dup = true;
                    break;
                }
            }
            if (dup) {
                continue;
            }
            cJSON *o = cJSON_CreateObject();
            cJSON_AddStringToObject(o, "ssid", ssid);
            cJSON_AddNumberToObject(o, "rssi", recs[i].rssi);
            cJSON_AddBoolToObject(o, "secure", recs[i].authmode != WIFI_AUTH_OPEN);
            cJSON_AddNumberToObject(o, "channel", recs[i].primary);
            cJSON_AddItemToArray(arr, o);
        }
    }
    free(recs);
    return arr;
}

void wifi_get_mac_str(char out[18])
{
    uint8_t mac[6] = {0};
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    snprintf(out, 18, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void wifi_get_ip_str(char out[16])
{
    esp_netif_ip_info_t ip = {0};
    if (s_sta_netif && (xEventGroupGetBits(s_events) & GOT_IP_BIT)) {
        esp_netif_get_ip_info(s_sta_netif, &ip);
    }
    snprintf(out, 16, IPSTR, IP2STR(&ip.ip));
}

int wifi_get_rssi(void)
{
    wifi_ap_record_t ap;
    return esp_wifi_sta_get_ap_info(&ap) == ESP_OK ? ap.rssi : 0;
}
