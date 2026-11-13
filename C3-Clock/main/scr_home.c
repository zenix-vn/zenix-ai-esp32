// Home: big clock, date, weather card, Wi-Fi status. Any press opens the app screen.
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "esp_timer.h"
#include "lunar.h"
#include "ui.h"

static lv_obj_t *l_hh, *l_colon, *l_mm, *l_ampm, *l_lunar, *l_date, *l_city, *wifi_bars[4], *l_status;
static lv_obj_t *l_wx_icon, *l_temp, *l_desc, *l_hilo, *l_hum, *l_wind, *l_wx_wait, *portal;

static void create(lv_obj_t *scr) {
    lv_obj_t *pin = ui_label(scr, &font_ic_16, C_ACCENT, IC_PIN);
    lv_obj_set_pos(pin, 12, 9);
    l_city = ui_label(scr, &font_vi_15, C_SUB, "Zenix Clock");
    lv_obj_set_pos(l_city, 32, 9);

    // Wi-Fi signal: 4 white bars at the top-right corner
    for (int i = 0; i < 4; i++) {
        wifi_bars[i] = ui_box(scr, 0, 0, 4, 4 + i * 4, C_TEXT, 1);
        lv_obj_align(wifi_bars[i], LV_ALIGN_TOP_RIGHT, -12 - (3 - i) * 6, 25 - (4 + i * 4));
    }
    l_status = ui_label(scr, &font_vi_15, C_WARN, "");   // running stopwatch / timer
    lv_obj_align(l_status, LV_ALIGN_TOP_RIGHT, -46, 9);

    lv_obj_t *row = lv_obj_create(scr);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 30);
    l_hh = ui_label(row, &font_clock_92, C_TEXT, "--");
    l_colon = ui_label(row, &font_clock_92, C_ACCENT, ":");
    l_mm = ui_label(row, &font_clock_92, C_TEXT, "--");
    lv_obj_set_width(l_hh, 124);
    lv_obj_set_style_text_align(l_hh, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_width(l_mm, 124);
    lv_obj_set_style_text_align(l_mm, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_pad_hor(l_colon, 4, 0);
    l_ampm = ui_label(scr, &font_vi_15, C_SUB, "");
    lv_obj_align(l_ampm, LV_ALIGN_TOP_RIGHT, -8, 80);

    l_date = ui_label(scr, &font_vi_20, 0xCBD5E1, "Đang đồng bộ giờ...");
    lv_obj_align(l_date, LV_ALIGN_TOP_MID, 0, 102);
    l_lunar = ui_label(scr, &font_vi_15, 0xFB923C, "");
    lv_obj_set_width(l_lunar, 300);
    lv_obj_set_style_text_align(l_lunar, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(l_lunar, LV_LABEL_LONG_DOT);
    lv_obj_align(l_lunar, LV_ALIGN_TOP_MID, 0, 132);

    lv_obj_t *card = ui_box(scr, 10, SCR_H - 8 - 68, 300, 68, C_CARD, 16);
    lv_obj_set_style_border_color(card, lv_color_hex(C_BORDER), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    l_wx_icon = ui_label(card, &font_wx_48, 0xCBD5E1, IC_CLOUD);
    lv_obj_align(l_wx_icon, LV_ALIGN_LEFT_MID, 12, 0);
    l_temp = ui_label(card, &font_temp_44, C_TEXT, "--°");
    lv_obj_align(l_temp, LV_ALIGN_LEFT_MID, 78, 0);
    l_desc = ui_label(card, &font_vi_15, C_TEXT, "");
    lv_obj_set_pos(l_desc, 172, 5);
    l_hilo = ui_label(card, &font_vi_15, C_SUB, "");
    lv_obj_set_pos(l_hilo, 172, 25);
    lv_obj_t *ic_drop = ui_label(card, &font_ic_16, 0x60A5FA, IC_DROP);
    lv_obj_set_pos(ic_drop, 172, 45);
    l_hum = ui_label(card, &font_vi_15, C_SUB, "--%");
    lv_obj_set_pos(l_hum, 186, 45);
    lv_obj_t *ic_wind = ui_label(card, &font_ic_16, 0x94A3B8, IC_WIND);
    lv_obj_set_pos(ic_wind, 228, 45);
    l_wind = ui_label(card, &font_vi_15, C_SUB, "--");
    lv_obj_set_pos(l_wind, 250, 45);
    l_wx_wait = ui_label(card, &font_vi_15, C_SUB, "Đang tải thời tiết...");
    lv_obj_align(l_wx_wait, LV_ALIGN_RIGHT_MID, -14, 0);

    // Wi-Fi setup overlay
    portal = ui_box(scr, 0, 0, SCR_W, SCR_H, C_BG_BOT, 0);
    lv_obj_t *pi = ui_label(portal, &font_ic_16, C_PORTAL, IC_WIFI);
    lv_obj_align(pi, LV_ALIGN_TOP_MID, 0, 28);
    lv_obj_t *pt = ui_label(portal, &font_vi_20, C_TEXT, "Cài đặt WiFi");
    lv_obj_align(pt, LV_ALIGN_TOP_MID, 0, 52);
    lv_obj_t *p1 = ui_label(portal, &font_vi_15, C_SUB, "1. Trên điện thoại, kết nối WiFi:");
    lv_obj_align(p1, LV_ALIGN_TOP_MID, 0, 96);
    lv_obj_t *p2 = ui_label(portal, &font_vi_20, C_ACCENT, "Zenix-Clock");
    lv_obj_align(p2, LV_ALIGN_TOP_MID, 0, 116);
    lv_obj_t *p3 = ui_label(portal, &font_vi_15, C_SUB, "2. Mở trình duyệt, vào địa chỉ:");
    lv_obj_align(p3, LV_ALIGN_TOP_MID, 0, 152);
    lv_obj_t *p4 = ui_label(portal, &font_vi_20, C_ACCENT, "192.168.4.1");
    lv_obj_align(p4, LV_ALIGN_TOP_MID, 0, 172);
    lv_obj_add_flag(portal, LV_OBJ_FLAG_HIDDEN);
}

static void tick(void) {
    static const char *wd[] = {"Chủ Nhật", "Thứ Hai", "Thứ Ba", "Thứ Tư", "Thứ Năm", "Thứ Sáu", "Thứ Bảy"};
    app_state_t st;
    state_lock(); st = g_state; state_unlock();
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    int64_t ms = esp_timer_get_time() / 1000;
    char b[96];

    if (st.time_synced) {
        int h = tm.tm_hour;
        if (g_cfg.h12) { h %= 12; if (!h) h = 12; }
        snprintf(b, sizeof b, "%02d", h); lv_label_set_text(l_hh, b);
        snprintf(b, sizeof b, "%02d", tm.tm_min); lv_label_set_text(l_mm, b);
        lv_label_set_text(l_ampm, g_cfg.h12 ? (tm.tm_hour < 12 ? "SA" : "CH") : "");
        snprintf(b, sizeof b, "%s, %d tháng %d, %d", wd[tm.tm_wday], tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900);
        lv_label_set_text(l_date, b);
        static int lunar_for_day = -1;   // recompute only when the date changes
        if (lunar_for_day != tm.tm_yday || !lv_label_get_text(l_lunar)[0]) {
            lunar_for_day = tm.tm_yday;
            lunar_t l;
            char cy[20];
            lunar_from_solar(tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900, &l);
            lunar_can_chi_year(l.year, cy, sizeof cy);
            const char *ev = lunar_holiday(tm.tm_mday, tm.tm_mon + 1, &l);
            snprintf(b, sizeof b, "Âm lịch %d tháng %d%s, %s%s%s", l.day, l.month, l.leap ? " nhuận" : "", cy,
                     ev ? " · " : "", ev ? ev : "");
            lv_label_set_text(l_lunar, b);
        }
        lv_obj_set_style_opa(l_colon, (tm.tm_sec & 1) ? LV_OPA_40 : LV_OPA_COVER, 0);
    } else {
        lv_obj_set_style_opa(l_colon, (ms / 500) & 1 ? LV_OPA_40 : LV_OPA_COVER, 0);
    }
    if (st.city[0]) lv_label_set_text(l_city, st.city);

    // lit bars = signal level; all faint when offline, blinking while connecting
    int lvl = 0;
    if (st.wifi == WIFI_ST_CONNECTED) lvl = st.rssi > -55 ? 4 : st.rssi > -67 ? 3 : st.rssi > -78 ? 2 : 1;
    bool blink = st.wifi == WIFI_ST_CONNECTING && ((ms / 500) & 1);
    for (int i = 0; i < 4; i++)
        lv_obj_set_style_bg_opa(wifi_bars[i], i < lvl ? LV_OPA_COVER : blink ? LV_OPA_60 : LV_OPA_20, 0);

    lv_label_set_text(l_status, clock_status(b, sizeof b) ? b : "");

    if (st.wx_valid) {
        const char *icon, *desc;
        uint32_t col;
        weather_look(st.code, st.is_day, &icon, &col, &desc);
        lv_label_set_text(l_wx_icon, icon);
        lv_obj_set_style_text_color(l_wx_icon, lv_color_hex(col), 0);
        snprintf(b, sizeof b, "%d°", (int)(st.temp + (st.temp >= 0 ? 0.5f : -0.5f)));
        lv_label_set_text(l_temp, b);
        lv_label_set_text(l_desc, desc);
        snprintf(b, sizeof b, "Cao %d°  Thấp %d°", (int)(st.tmax + 0.5f), (int)(st.tmin + 0.5f));
        lv_label_set_text(l_hilo, b);
        snprintf(b, sizeof b, "%d%%", st.humidity); lv_label_set_text(l_hum, b);
        snprintf(b, sizeof b, "%dkm/h", (int)(st.wind + 0.5f)); lv_label_set_text(l_wind, b);
        lv_obj_add_flag(l_wx_wait, LV_OBJ_FLAG_HIDDEN);
    }
    if (st.wifi == WIFI_ST_PORTAL) lv_obj_remove_flag(portal, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(portal, LV_OBJ_FLAG_HIDDEN);
}

static bool on_btn(btn_evt_t e) {
    if (e == BTN_SHORT || e == BTN_LONG) ui_go(&SCR_APPS, LV_SCREEN_LOAD_ANIM_MOVE_LEFT);
    return true;
}

const screen_t SCR_HOME = {.create = create, .tick = tick, .on_btn = on_btn};
