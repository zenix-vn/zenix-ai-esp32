#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_lcd_panel_ops.h"

#define SCR_W 320
#define SCR_H 240
#define FW_VERSION "1.1.0"

typedef enum { WIFI_ST_CONNECTING, WIFI_ST_CONNECTED, WIFI_ST_PORTAL } wifi_state_t;

typedef struct { int64_t t; float temp; int code; int pop; } wx_hour_t;
typedef struct { int64_t t; int code; float tmax, tmin; int pop; float uv; } wx_day_t;

typedef struct {
    wifi_state_t wifi;
    int rssi;
    char ssid[33];
    char ip[16];
    bool time_synced;
    // current weather
    bool wx_valid;
    float temp, feels, wind, uv;
    int humidity, code;
    bool is_day;
    float tmax, tmin;
    int64_t sunrise, sunset;
    // air quality
    bool aq_valid;
    int aqi;
    float pm25;
    // forecasts
    wx_hour_t hours[8];   // every 3 h over the next 24 h
    int nhours;
    wx_day_t days[7];
    int ndays;
    int64_t wx_updated;   // unix time of last successful fetch
    char city[40];
} app_state_t;

extern app_state_t g_state;
void state_lock(void);
void state_unlock(void);

/* settings.c */
typedef struct { uint8_t h12, auto_home, rot; } settings_t;
extern settings_t g_cfg;
void settings_load(void);
void settings_save(void);
int settings_auto_home_s(void);          // 0 = never
const char *settings_auto_home_text(void);

/* lcd.c */
esp_lcd_panel_handle_t lcd_start(void);
void lcd_set_rotation(int rot);
void lcd_set_flush_done_cb(void (*cb)(void));
void lcd_reinit(int rot);

/* net.c */
void net_start(void);
void net_forget_wifi(void);
void net_refresh(void);                  // ask for an immediate weather update

/* ui_core.c */
void ui_init(esp_lcd_panel_handle_t panel);
void ui_run(void);
