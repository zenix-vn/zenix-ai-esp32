// Wi-Fi (STA + setup portal), SNTP time, location via ip-api.com, weather via open-meteo.com.
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "app.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs.h"

#define AP_SSID "Zenix-Clock"
#define BIT_GOT_IP BIT0
#define BIT_REFRESH BIT1

static const char *TAG = "net";
static EventGroupHandle_t s_ev;
static httpd_handle_t s_httpd;
static bool s_portal;
static bool s_have_creds;

/* ---------------- NVS credentials ---------------- */
static bool load_creds(char *ssid, char *pass) {
    nvs_handle_t h;
    if (nvs_open("wifi", NVS_READONLY, &h) != ESP_OK) return false;
    size_t a = 33, b = 65;
    bool ok = nvs_get_str(h, "ssid", ssid, &a) == ESP_OK && nvs_get_str(h, "pass", pass, &b) == ESP_OK && ssid[0];
    nvs_close(h);
    return ok;
}

static void save_creds(const char *ssid, const char *pass) {
    nvs_handle_t h;
    if (nvs_open("wifi", NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_str(h, "ssid", ssid);
    nvs_set_str(h, "pass", pass);
    nvs_commit(h);
    nvs_close(h);
}

void net_forget_wifi(void) {
    nvs_handle_t h;
    if (nvs_open("wifi", NVS_READWRITE, &h) == ESP_OK) { nvs_erase_all(h); nvs_commit(h); nvs_close(h); }
    esp_restart();
}

/* ---------------- setup portal ---------------- */
static wifi_ap_record_t s_aps[16];
static int s_nap = -1;   // -1 = never scanned

static void do_scan(void) {
    wifi_scan_config_t sc = {.show_hidden = false};
    if (esp_wifi_scan_start(&sc, true) != ESP_OK) return;
    uint16_t n = 24;
    wifi_ap_record_t *recs = calloc(n, sizeof *recs);
    if (!recs) return;
    if (esp_wifi_scan_get_ap_records(&n, recs) == ESP_OK) {
        s_nap = 0;
        for (int i = 0; i < n && s_nap < 16; i++) {      // records come sorted by RSSI; drop duplicates
            if (!recs[i].ssid[0]) continue;
            bool dup = false;
            for (int j = 0; j < s_nap; j++) dup |= !strcmp((char *)s_aps[j].ssid, (char *)recs[i].ssid);
            if (!dup) s_aps[s_nap++] = recs[i];
        }
    }
    free(recs);
}

static const char PAGE_HEAD[] =
    "<!doctype html><html lang='vi'><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'><title>Zenix Clock</title><style>"
    "*{box-sizing:border-box}body{font-family:system-ui,-apple-system,sans-serif;background:#0b1020;color:#e5e9f0;margin:0}"
    "main{max-width:420px;margin:0 auto;padding:24px 16px 40px}h1{font-size:22px;margin:0 0 4px}"
    ".sub{color:#8a94a6;margin:0 0 18px;font-size:14px}"
    ".hd{display:flex;justify-content:space-between;align-items:center;margin:0 0 8px;color:#c7ced9;font-size:14px}"
    ".hd a{color:#38bdf8;text-decoration:none;font-size:14px}"
    ".list{background:#111a2e;border:1px solid #1e293b;border-radius:14px;overflow:hidden}"
    ".net{display:flex;align-items:center;gap:12px;width:100%;padding:14px;border:0;border-bottom:1px solid #1e293b;"
    "background:none;color:#e5e9f0;font-size:16px;text-align:left;cursor:pointer}"
    ".net:last-child{border-bottom:0}.net.on{background:#16233d}.net .n{flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}"
    ".bars{display:flex;align-items:flex-end;gap:2px;height:14px}.bars i{width:3px;background:#334155;border-radius:1px}"
    ".bars i.f{background:#e5e9f0}.lk{color:#8a94a6;font-size:13px}"
    "#f{display:none;margin-top:18px;background:#111a2e;border:1px solid #1e293b;border-radius:14px;padding:16px}"
    "#f b{display:block;font-size:17px;margin-bottom:12px;word-break:break-all}"
    "label{display:block;font-size:14px;margin:10px 0 6px;color:#c7ced9}"
    "input{width:100%;padding:12px;border-radius:10px;border:1px solid #26314a;background:#0b1020;color:#e5e9f0;font-size:16px}"
    "button.go{margin-top:16px;width:100%;padding:13px;border:0;border-radius:10px;background:#38bdf8;color:#04121f;"
    "font-size:16px;font-weight:600}.empty{padding:18px;color:#8a94a6;text-align:center}"
    ".man{display:block;margin-top:14px;color:#8a94a6;font-size:14px;text-align:center;background:none;border:0;width:100%}"
    "</style></head><body><main><h1>Zenix Clock</h1><p class='sub'>Chọn mạng WiFi rồi nhập mật khẩu</p>"
    "<div class='hd'><span>Mạng xung quanh</span><a href='/?scan=1'>&#8635; Quét lại</a></div><div class='list'>";

static const char PAGE_TAIL[] =
    "</div><button type='button' class='man' onclick='man()'>Mạng ẩn? Nhập tên thủ công</button>"
    "<form id='f' method='post' action='/save'><b id='t'></b>"
    "<div id='mw' style='display:none'><label>Tên WiFi</label><input name='s' id='s' autocomplete='off'></div>"
    "<div id='pw'><label>Mật khẩu</label><input name='p' id='p' type='password' autocomplete='current-password'></div>"
    "<button class='go' type='submit'>Kết nối</button></form>"
    "<script>"
    "var f=document.getElementById('f'),s=document.getElementById('s'),p=document.getElementById('p');"
    "function pick(e){document.querySelectorAll('.net').forEach(function(x){x.classList.remove('on')});"
    "e.classList.add('on');s.value=e.dataset.s;document.getElementById('t').textContent=e.dataset.s;"
    "document.getElementById('mw').style.display='none';"
    "var l=e.dataset.l=='1';document.getElementById('pw').style.display=l?'block':'none';if(!l)p.value='';"
    "f.style.display='block';f.scrollIntoView({behavior:'smooth'});if(l)p.focus();}"
    "function man(){document.querySelectorAll('.net').forEach(function(x){x.classList.remove('on')});"
    "s.value='';document.getElementById('t').textContent='Mạng ẩn';document.getElementById('mw').style.display='block';"
    "document.getElementById('pw').style.display='block';f.style.display='block';s.focus();}"
    "</script></main></body></html>";

static void html_escape(const char *in, char *out, size_t cap) {
    size_t o = 0;
    for (; *in && o + 7 < cap; in++) {
        const char *r = NULL;
        switch (*in) {
            case '&': r = "&amp;"; break;
            case '<': r = "&lt;"; break;
            case '>': r = "&gt;"; break;
            case '"': r = "&quot;"; break;
            case '\'': r = "&#39;"; break;
        }
        if (r) { size_t n = strlen(r); memcpy(out + o, r, n); o += n; }
        else out[o++] = *in;
    }
    out[o] = 0;
}

static esp_err_t root_get(httpd_req_t *req) {
    char q[16];
    bool rescan = httpd_req_get_url_query_str(req, q, sizeof q) == ESP_OK && strstr(q, "scan");
    if (s_nap < 0 || rescan) do_scan();

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send_chunk(req, PAGE_HEAD, HTTPD_RESP_USE_STRLEN);
    if (s_nap <= 0) {
        httpd_resp_send_chunk(req, "<div class='empty'>Không tìm thấy mạng nào. Bấm Quét lại.</div>", HTTPD_RESP_USE_STRLEN);
    }
    char esc[200], row[520];
    for (int i = 0; i < s_nap; i++) {
        int r = s_aps[i].rssi, lvl = r > -55 ? 4 : r > -67 ? 3 : r > -78 ? 2 : 1;
        bool locked = s_aps[i].authmode != WIFI_AUTH_OPEN;
        html_escape((char *)s_aps[i].ssid, esc, sizeof esc);
        snprintf(row, sizeof row,
                 "<button type='button' class='net' data-s=\"%s\" data-l='%d' onclick='pick(this)'>"
                 "<span class='bars'><i class='%s' style='height:4px'></i><i class='%s' style='height:7px'></i>"
                 "<i class='%s' style='height:10px'></i><i class='%s' style='height:14px'></i></span>"
                 "<span class='n'>%s</span><span class='lk'>%s</span></button>",
                 esc, locked, lvl >= 1 ? "f" : "", lvl >= 2 ? "f" : "", lvl >= 3 ? "f" : "", lvl >= 4 ? "f" : "",
                 esc, locked ? "&#128274;" : "");
        httpd_resp_send_chunk(req, row, HTTPD_RESP_USE_STRLEN);
    }
    httpd_resp_send_chunk(req, PAGE_TAIL, HTTPD_RESP_USE_STRLEN);
    return httpd_resp_send_chunk(req, NULL, 0);
}

static void url_decode(char *s) {
    char *o = s;
    for (; *s; s++) {
        if (*s == '+') *o++ = ' ';
        else if (*s == '%' && s[1] && s[2]) { char h[3] = {s[1], s[2], 0}; *o++ = (char)strtol(h, NULL, 16); s += 2; }
        else *o++ = *s;
    }
    *o = 0;
}

static esp_err_t save_post(httpd_req_t *req) {
    // URL-encoded values can be 3x longer than the decoded SSID (32) / password (64).
    char body[512] = {0}, ssid[100] = {0}, pass[200] = {0};
    int len = httpd_req_recv(req, body, sizeof body - 1);
    if (len <= 0) return ESP_FAIL;
    body[len] = 0;
    httpd_query_key_value(body, "s", ssid, sizeof ssid);
    httpd_query_key_value(body, "p", pass, sizeof pass);
    url_decode(ssid);
    url_decode(pass);
    ssid[32] = 0;
    pass[64] = 0;
    if (!ssid[0]) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing ssid");
    save_creds(ssid, pass);
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_sendstr(req, "<!doctype html><meta charset='utf-8'><meta name='viewport' content='width=device-width'>"
                            "<body style='font-family:system-ui;background:#0b1020;color:#e5e9f0;padding:30px'>"
                            "<h2>Đã lưu</h2><p>Đồng hồ đang khởi động lại và kết nối WiFi.</p></body>");
    vTaskDelay(pdMS_TO_TICKS(1500));
    esp_restart();
    return ESP_OK;
}

static esp_err_t redirect_get(httpd_req_t *req) {   // captive-portal style redirect for phones
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    return httpd_resp_send(req, NULL, 0);
}

static void start_portal(void) {
    if (s_portal) return;
    s_portal = true;
    wifi_config_t ap = {.ap = {.ssid = AP_SSID, .ssid_len = strlen(AP_SSID), .max_connection = 2,
                               .authmode = WIFI_AUTH_OPEN, .channel = 1}};
    esp_wifi_set_mode(WIFI_MODE_APSTA);
    esp_wifi_set_config(WIFI_IF_AP, &ap);
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.uri_match_fn = httpd_uri_match_wildcard;
    cfg.stack_size = 6144;
    if (httpd_start(&s_httpd, &cfg) == ESP_OK) {
        httpd_uri_t u1 = {.uri = "/", .method = HTTP_GET, .handler = root_get};
        httpd_uri_t u2 = {.uri = "/save", .method = HTTP_POST, .handler = save_post};
        httpd_uri_t u3 = {.uri = "/*", .method = HTTP_GET, .handler = redirect_get};
        httpd_register_uri_handler(s_httpd, &u1);
        httpd_register_uri_handler(s_httpd, &u2);
        httpd_register_uri_handler(s_httpd, &u3);
    }
    state_lock(); g_state.wifi = WIFI_ST_PORTAL; state_unlock();
    ESP_LOGI(TAG, "setup portal: join '%s', open http://192.168.4.1", AP_SSID);
    do_scan();   // so the first page load already has the network list
}

/* ---------------- Wi-Fi events ---------------- */
static void wifi_evt(void *arg, esp_event_base_t base, int32_t id, void *data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        if (s_have_creds) esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_ev, BIT_GOT_IP);
        state_lock(); if (g_state.wifi == WIFI_ST_CONNECTED) g_state.wifi = WIFI_ST_CONNECTING; state_unlock();
        if (s_have_creds) esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *ev = data;
        xEventGroupSetBits(s_ev, BIT_GOT_IP);
        state_lock();
        g_state.wifi = WIFI_ST_CONNECTED;
        snprintf(g_state.ip, sizeof g_state.ip, IPSTR, IP2STR(&ev->ip_info.ip));
        state_unlock();
    }
}

/* ---------------- HTTP + tiny JSON helpers ---------------- */
static int http_get(const char *url, char *buf, int cap) {
    esp_http_client_config_t cfg = {.url = url, .timeout_ms = 8000};
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    int total = -1;
    if (esp_http_client_open(c, 0) == ESP_OK) {
        esp_http_client_fetch_headers(c);
        total = 0;
        int r;
        while (total < cap - 1 && (r = esp_http_client_read(c, buf + total, cap - 1 - total)) > 0) total += r;
        buf[total] = 0;
        if (esp_http_client_get_status_code(c) != 200) total = -1;
    }
    esp_http_client_cleanup(c);
    return total;
}

// Find "key": inside s (after optional section marker) and parse number; [ ] arrays take first element.
static bool json_num(const char *s, const char *section, const char *key, double *out) {
    if (section && !(s = strstr(s, section))) return false;
    char pat[48];
    snprintf(pat, sizeof pat, "\"%s\":", key);
    const char *p = strstr(s, pat);
    if (!p) return false;
    p += strlen(pat);
    while (*p == ' ' || *p == '[') p++;
    char *end;
    *out = strtod(p, &end);
    return end != p;
}

static bool json_str(const char *s, const char *key, char *out, size_t cap) {
    char pat[48];
    snprintf(pat, sizeof pat, "\"%s\":\"", key);
    const char *p = strstr(s, pat);
    if (!p) return false;
    p += strlen(pat);
    size_t i = 0;
    while (*p && *p != '"' && i < cap - 1) out[i++] = *p++;
    out[i] = 0;
    return true;
}

// Parse a JSON number array "key":[..] inside section; null entries become NAN. Returns count.
static int json_arr(const char *s, const char *section, const char *key, double *out, int max) {
    if (section && !(s = strstr(s, section))) return 0;
    char pat[48];
    snprintf(pat, sizeof pat, "\"%s\":[", key);
    const char *p = strstr(s, pat);
    if (!p) return 0;
    p += strlen(pat);
    int n = 0;
    while (*p && *p != ']' && n < max) {
        if (!strncmp(p, "null", 4)) { out[n++] = NAN; p += 4; }
        else { char *e; out[n++] = strtod(p, &e); if (e == p) break; p = e; }
        while (*p == ',' || *p == ' ') p++;
    }
    return n;
}

void net_refresh(void) { if (s_ev) xEventGroupSetBits(s_ev, BIT_REFRESH); }

static void locate(char *buf, int cap, double *lat, double *lon) {
    if (http_get("http://ip-api.com/json/?fields=status,city,lat,lon,offset", buf, cap) <= 0) return;
    double off;
    char city[40];
    json_num(buf, NULL, "lat", lat);
    json_num(buf, NULL, "lon", lon);
    if (json_num(buf, NULL, "offset", &off)) {
        int o = (int)off, a = abs(o);
        char tz[32];
        snprintf(tz, sizeof tz, "UTC%c%d:%02d", o >= 0 ? '-' : '+', a / 3600, (a % 3600) / 60);
        setenv("TZ", tz, 1);
        tzset();
    }
    if (json_str(buf, "city", city, sizeof city)) { state_lock(); strcpy(g_state.city, city); state_unlock(); }
    ESP_LOGI(TAG, "location %.2f,%.2f %s", *lat, *lon, g_state.city);
}

static bool fetch_weather(char *buf, int cap, double lat, double lon) {
    char url[600];
    snprintf(url, sizeof url,
             "http://api.open-meteo.com/v1/forecast?latitude=%.3f&longitude=%.3f"
             "&current=temperature_2m,apparent_temperature,relative_humidity_2m,weather_code,wind_speed_10m,is_day,uv_index"
             "&hourly=temperature_2m,weather_code,precipitation_probability"
             "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max,sunrise,sunset,uv_index_max"
             "&timezone=auto&timeformat=unixtime&forecast_days=7&forecast_hours=24",
             lat, lon);
    int len = http_get(url, buf, cap);
    if (len <= 0) { ESP_LOGW(TAG, "weather fetch failed"); return false; }
    const char *cur = "\"current\":{", *hr = "\"hourly\":{", *dy = "\"daily\":{";
    double t, v;
    if (!json_num(buf, cur, "temperature_2m", &t)) { ESP_LOGW(TAG, "weather parse failed (%d bytes)", len); return false; }

    static double a_t[24], a_temp[24], a_code[24], a_pop[24];
    static double d_t[7], d_code[7], d_max[7], d_min[7], d_pop[7], d_rise[7], d_set[7], d_uv[7];
    int nh = json_arr(buf, hr, "time", a_t, 24);
    json_arr(buf, hr, "temperature_2m", a_temp, 24);
    json_arr(buf, hr, "weather_code", a_code, 24);
    json_arr(buf, hr, "precipitation_probability", a_pop, 24);
    int nd = json_arr(buf, dy, "time", d_t, 7);
    json_arr(buf, dy, "weather_code", d_code, 7);
    json_arr(buf, dy, "temperature_2m_max", d_max, 7);
    json_arr(buf, dy, "temperature_2m_min", d_min, 7);
    json_arr(buf, dy, "precipitation_probability_max", d_pop, 7);
    json_arr(buf, dy, "sunrise", d_rise, 7);
    json_arr(buf, dy, "sunset", d_set, 7);
    json_arr(buf, dy, "uv_index_max", d_uv, 7);

    state_lock();
    g_state.temp = t;
    if (json_num(buf, cur, "apparent_temperature", &v)) g_state.feels = v;
    if (json_num(buf, cur, "relative_humidity_2m", &v)) g_state.humidity = (int)v;
    if (json_num(buf, cur, "weather_code", &v)) g_state.code = (int)v;
    if (json_num(buf, cur, "wind_speed_10m", &v)) g_state.wind = v;
    if (json_num(buf, cur, "is_day", &v)) g_state.is_day = v > 0.5;
    if (json_num(buf, cur, "uv_index", &v)) g_state.uv = v;
    g_state.nhours = 0;
    for (int i = 0; i < nh && g_state.nhours < 8; i += 3) {
        wx_hour_t *h = &g_state.hours[g_state.nhours++];
        h->t = (int64_t)a_t[i];
        h->temp = a_temp[i];
        h->code = isnan(a_code[i]) ? 3 : (int)a_code[i];
        h->pop = isnan(a_pop[i]) ? 0 : (int)a_pop[i];
    }
    g_state.ndays = nd;
    for (int i = 0; i < nd; i++) {
        wx_day_t *d = &g_state.days[i];
        d->t = (int64_t)d_t[i];
        d->code = isnan(d_code[i]) ? 3 : (int)d_code[i];
        d->tmax = d_max[i];
        d->tmin = d_min[i];
        d->pop = isnan(d_pop[i]) ? 0 : (int)d_pop[i];
        d->uv = d_uv[i];
    }
    if (nd > 0) {
        g_state.tmax = d_max[0];
        g_state.tmin = d_min[0];
        g_state.sunrise = (int64_t)d_rise[0];
        g_state.sunset = (int64_t)d_set[0];
    }
    g_state.wx_valid = true;
    g_state.wx_updated = time(NULL);
    state_unlock();
    ESP_LOGI(TAG, "weather %.1fC code %d, %d hours, %d days", t, g_state.code, g_state.nhours, nd);
    return true;
}

static void fetch_air(char *buf, int cap, double lat, double lon) {
    char url[200];
    snprintf(url, sizeof url,
             "http://air-quality-api.open-meteo.com/v1/air-quality?latitude=%.3f&longitude=%.3f&current=us_aqi,pm2_5",
             lat, lon);
    double a, p;
    if (http_get(url, buf, cap) > 0 && json_num(buf, "\"current\":{", "us_aqi", &a)) {
        state_lock();
        g_state.aqi = (int)a;
        if (json_num(buf, "\"current\":{", "pm2_5", &p)) g_state.pm25 = p;
        g_state.aq_valid = true;
        g_state.wx_updated = time(NULL);
        state_unlock();
    }
}

static void net_task(void *arg) {
    const int cap = 8192;
    char *buf = malloc(cap);
    double lat = 21.03, lon = 105.85;   // fallback: Hà Nội
    bool located = false;
    while (1) {
        xEventGroupWaitBits(s_ev, BIT_GOT_IP, false, true, portMAX_DELAY);
        wifi_ap_record_t ap;
        if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) { state_lock(); g_state.rssi = ap.rssi; state_unlock(); }

        if (!located) { locate(buf, cap, &lat, &lon); located = g_state.city[0] != 0; }
        int wait_s = fetch_weather(buf, cap, lat, lon) ? 15 * 60 : 60;
        fetch_air(buf, cap, lat, lon);

        for (int i = 0; i < wait_s / 10; i++) {   // refresh RSSI every 10 s; wake early on request
            if (xEventGroupWaitBits(s_ev, BIT_REFRESH, true, false, pdMS_TO_TICKS(10000)) & BIT_REFRESH) break;
            if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) { state_lock(); g_state.rssi = ap.rssi; state_unlock(); }
        }
    }
}

static void time_sync_cb(struct timeval *tv) {
    state_lock(); g_state.time_synced = true; state_unlock();
    ESP_LOGI(TAG, "time synced");
}

void net_start(void) {
    setenv("TZ", "UTC-7:00", 1);   // Vietnam until ip-api tells otherwise
    tzset();
    s_ev = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_evt, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_evt, NULL);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);

    char ssid[33] = {0}, pass[65] = {0};
    bool have = load_creds(ssid, pass);
    s_have_creds = have;
    if (have) {
        wifi_config_t sta = {0};
        memcpy(sta.sta.ssid, ssid, strlen(ssid));
        memcpy(sta.sta.password, pass, strlen(pass));
        sta.sta.threshold.authmode = pass[0] ? WIFI_AUTH_WPA_PSK : WIFI_AUTH_OPEN;
        esp_wifi_set_mode(WIFI_MODE_STA);
        esp_wifi_set_config(WIFI_IF_STA, &sta);
        state_lock(); strcpy(g_state.ssid, ssid); state_unlock();
    } else {
        esp_wifi_set_mode(WIFI_MODE_APSTA);
    }
    ESP_ERROR_CHECK(esp_wifi_start());
    // Limit TX power (unit 0.25 dBm -> 11 dBm): cuts current spikes that brown out weak USB supplies.
    esp_wifi_set_max_tx_power(44);
    if (!have) start_portal();

    esp_sntp_config_t sntp = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    sntp.sync_cb = time_sync_cb;
    esp_netif_sntp_init(&sntp);

    xTaskCreate(net_task, "net", 6144, NULL, 4, NULL);

    // Saved network unreachable for 40 s after boot -> also open the setup portal.
    if (have && !(xEventGroupWaitBits(s_ev, BIT_GOT_IP, false, true, pdMS_TO_TICKS(40000)) & BIT_GOT_IP))
        start_portal();
}
