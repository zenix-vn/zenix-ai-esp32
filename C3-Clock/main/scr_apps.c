// App launcher: one row of 4 rounded-square icons; the focused one floats up with a coloured glow.
// Short = next icon, long = open, hold = Home.
#include <stdio.h>
#include <time.h>
#include "lunar.h"
#include "ui.h"

#define SLOT_W 76
#define ICON   56
#define ICON_Y 64
#define LIFT   10

typedef struct { const char *icon; uint32_t color; const char *name; const screen_t *scr; } app_t;
static const app_t APPS[4] = {
    {IC_CLOUD_SUN, 0xF59E0B, "Thời tiết", &APP_WEATHER},
    {IC_CALENDAR, 0xEF4444, "Lịch", &APP_CAL},
    {IC_CLOCK, 0x6366F1, "Đồng hồ", &APP_CLOCK},
    {IC_COG, 0x64748B, "Cài đặt", &APP_SETTINGS},
};

static int s_focus;
static lv_obj_t *icons[4], *names[4], *s_info;

static void anim_y(void *obj, int32_t v) { lv_obj_set_style_translate_y(obj, v, 0); }

static void float_to(lv_obj_t *o, int32_t to) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, o);
    lv_anim_set_exec_cb(&a, anim_y);
    lv_anim_set_values(&a, lv_obj_get_style_translate_y(o, 0), to);
    lv_anim_set_duration(&a, 160);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

static void apply_focus(bool animate) {
    for (int i = 0; i < 4; i++) {
        bool f = i == s_focus;
        if (animate) float_to(icons[i], f ? -LIFT : 0);
        else lv_obj_set_style_translate_y(icons[i], f ? -LIFT : 0, 0);
        lv_obj_set_style_shadow_width(icons[i], f ? 22 : 0, 0);
        lv_obj_set_style_shadow_opa(icons[i], f ? LV_OPA_60 : LV_OPA_TRANSP, 0);
        lv_obj_set_style_text_color(names[i], lv_color_hex(f ? C_TEXT : C_DIM), 0);
    }
}

static void info_text(char *b, size_t n) {
    app_state_t st;
    state_lock(); st = g_state; state_unlock();
    switch (s_focus) {
        case 0:
            if (st.wx_valid) {
                const char *icon, *desc;
                uint32_t col;
                weather_look(st.code, st.is_day, &icon, &col, &desc);
                snprintf(b, n, "%d°C · %s", (int)(st.temp + 0.5f), desc);
            } else snprintf(b, n, "Chưa có dữ liệu thời tiết");
            break;
        case 1: {
            time_t now = time(NULL);
            struct tm tm;
            localtime_r(&now, &tm);
            if (!st.time_synced) { snprintf(b, n, "Lịch âm dương"); break; }
            lunar_t l;
            char cy[20];
            lunar_from_solar(tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900, &l);
            lunar_can_chi_year(l.year, cy, sizeof cy);
            snprintf(b, n, "Âm lịch %d/%d · %s", l.day, l.month, cy);
            break;
        }
        case 2:
            if (!clock_status(b, n)) snprintf(b, n, "Đồng hồ kim · Bấm giờ · Hẹn giờ");
            break;
        default:
            if (st.wifi == WIFI_ST_CONNECTED) snprintf(b, n, "WiFi: %s", st.ssid);
            else snprintf(b, n, "WiFi, hiển thị, thông tin");
            break;
    }
}

static void create(lv_obj_t *scr) {
    ui_title(scr, "Ứng dụng");
    int x0 = (SCR_W - 4 * SLOT_W) / 2;
    for (int i = 0; i < 4; i++) {
        int sx = x0 + i * SLOT_W;
        lv_obj_t *ic = ui_box(scr, sx + (SLOT_W - ICON) / 2, ICON_Y, ICON, ICON, APPS[i].color, 18);
        lv_obj_set_style_bg_grad_color(ic, lv_color_darken(lv_color_hex(APPS[i].color), LV_OPA_30), 0);
        lv_obj_set_style_bg_grad_dir(ic, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_shadow_color(ic, lv_color_hex(APPS[i].color), 0);
        lv_obj_set_style_shadow_offset_y(ic, 6, 0);
        lv_obj_t *g = ui_label(ic, &font_app_24, 0xFFFFFF, APPS[i].icon);
        lv_obj_center(g);
        icons[i] = ic;
        names[i] = ui_label(scr, &font_vi_15, C_DIM, APPS[i].name);
        lv_obj_set_width(names[i], SLOT_W);
        lv_obj_set_style_text_align(names[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(names[i], sx, ICON_Y + ICON + 10);
    }
    // info line for the focused app
    lv_obj_t *card = ui_box(scr, 16, 160, SCR_W - 32, 40, C_CARD, 12);
    s_info = ui_label(card, &font_vi_15, C_SUB, "");
    lv_obj_set_width(s_info, SCR_W - 56);
    lv_obj_set_style_text_align(s_info, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(s_info, LV_LABEL_LONG_DOT);
    lv_obj_center(s_info);
    apply_focus(false);
    ui_hint(scr, "Nhấn: chọn tiếp · Giữ: mở · Giữ lâu: Home");
}

static void tick(void) {
    char b[64];
    info_text(b, sizeof b);
    lv_label_set_text(s_info, b);
}

static bool on_btn(btn_evt_t e) {
    if (e == BTN_SHORT) { s_focus = (s_focus + 1) % 4; apply_focus(true); tick(); return true; }
    if (e == BTN_LONG) { ui_go(APPS[s_focus].scr, LV_SCREEN_LOAD_ANIM_MOVE_LEFT); return true; }
    return false;   // hold -> Home
}

const screen_t SCR_APPS = {.create = create, .tick = tick, .on_btn = on_btn};
