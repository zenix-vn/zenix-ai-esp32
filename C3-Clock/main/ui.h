#pragma once
#include <time.h>
#include "app.h"
#include "lvgl.h"

LV_FONT_DECLARE(font_clock_92);
LV_FONT_DECLARE(font_temp_44);
LV_FONT_DECLARE(font_vi_20);
LV_FONT_DECLARE(font_vi_15);
LV_FONT_DECLARE(font_tiny_11);
LV_FONT_DECLARE(font_wx_48);
LV_FONT_DECLARE(font_wx_24);
LV_FONT_DECLARE(font_app_24);
LV_FONT_DECLARE(font_ic_16);

/* FontAwesome glyphs (UTF-8) */
#define IC_SUN        "\xEF\x86\x85"
#define IC_MOON       "\xEF\x86\x86"
#define IC_CLOUD_SUN  "\xEF\x9B\x84"
#define IC_CLOUD_MOON "\xEF\x9B\x83"
#define IC_CLOUD      "\xEF\x83\x82"
#define IC_SMOG       "\xEF\x9D\x9F"
#define IC_RAIN       "\xEF\x9C\xBD"
#define IC_SHOWERS    "\xEF\x9D\x80"
#define IC_SNOW       "\xEF\x8B\x9C"
#define IC_BOLT       "\xEF\x83\xA7"
#define IC_WIFI       "\xEF\x87\xAB"
#define IC_DROP       "\xEF\x81\x83"
#define IC_WIND       "\xEF\x9C\xAE"
#define IC_PIN        "\xEF\x8F\x85"
#define IC_UP         "\xEF\x81\xA2"
#define IC_DOWN       "\xEF\x81\xA3"
#define IC_STOPWATCH  "\xEF\x8B\xB2"
#define IC_HOURGLASS  "\xEF\x89\x92"
#define IC_CALENDAR   "\xEF\x81\xB3"
#define IC_CLOCK      "\xEF\x80\x97"
#define IC_COG        "\xEF\x80\x93"

/* palette */
#define C_BG_TOP  0x0F172A
#define C_BG_BOT  0x020617
#define C_TEXT    0xF1F5F9
#define C_SUB     0x94A3B8
#define C_DIM     0x64748B
#define C_ACCENT  0x38BDF8
#define C_CARD    0x111A2E
#define C_CARD_HI 0x1A2A47
#define C_BORDER  0x1E293B
#define C_OK      0x4ADE80
#define C_WARN    0xFBBF24
#define C_BAD     0xF87171
#define C_PORTAL  0x60A5FA

typedef enum { BTN_SHORT, BTN_LONG, BTN_HOLD } btn_evt_t;

typedef struct {
    void (*create)(lv_obj_t *scr);
    void (*destroy)(void);
    void (*tick)(void);                 // every 200 ms while visible
    bool (*on_btn)(btn_evt_t e);        // true = handled; unhandled BTN_HOLD goes back
    bool no_auto_home;
} screen_t;

extern const screen_t SCR_HOME, SCR_APPS, APP_WEATHER, APP_CAL, APP_CLOCK, APP_SETTINGS;

void ui_go(const screen_t *s, lv_screen_load_anim_t anim);
void ui_back(void);
void ui_apply_rotation(void);

/* widget helpers */
lv_obj_t *ui_label(lv_obj_t *parent, const lv_font_t *f, uint32_t color, const char *txt);
lv_obj_t *ui_box(lv_obj_t *parent, int x, int y, int w, int h, uint32_t bg, int radius);
lv_obj_t *ui_title(lv_obj_t *scr, const char *title);   // title left + live clock right
void ui_hint(lv_obj_t *scr, const char *txt);
lv_obj_t *ui_dots(lv_obj_t *scr, int n);
void ui_dots_set(lv_obj_t *dots, int idx);
void ui_fmt_hm(char *buf, size_t n, const struct tm *tm);   // respects 12/24 h setting
void weather_look(int code, bool day, const char **icon, uint32_t *color, const char **desc);

/* app_clock.c: timers keep running while other screens are shown */
bool clock_timer_poll(void);           // true once when the countdown reaches zero
void clock_timer_ack(void);
bool clock_status(char *buf, size_t n); // short text if stopwatch/timer is active
