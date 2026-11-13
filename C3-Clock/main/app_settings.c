// Settings app. Short = next item, long = change / run, hold = back.
#include <stdio.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_mac.h"
#include "ui.h"

#define NITEMS 5
static const char *NAMES[NITEMS] = {"Định dạng giờ", "Tự về màn chính", "Xoay màn hình", "Đổi mạng WiFi", "Thông tin thiết bị"};
static lv_obj_t *s_scr, *rows[NITEMS], *vals[NITEMS], *s_info;
static int s_focus;
static bool s_confirm;

static void refresh_values(void) {
    lv_label_set_text(vals[0], g_cfg.h12 ? "12 giờ" : "24 giờ");
    lv_label_set_text(vals[1], settings_auto_home_text());
    lv_label_set_text(vals[2], g_cfg.rot ? "Lật 180°" : "Bình thường");
    lv_label_set_text(vals[3], s_confirm ? "Giữ lần nữa để xoá" : "Xoá & cài lại");
    lv_obj_set_style_text_color(vals[3], lv_color_hex(s_confirm ? C_BAD : C_ACCENT), 0);
    lv_label_set_text(vals[4], "Xem");
    for (int i = 0; i < NITEMS; i++) {
        bool f = i == s_focus;
        lv_obj_set_style_bg_color(rows[i], lv_color_hex(f ? C_CARD_HI : C_CARD), 0);
        lv_obj_set_style_border_color(rows[i], lv_color_hex(f ? C_ACCENT : C_BORDER), 0);
        lv_obj_set_style_border_width(rows[i], f ? 2 : 1, 0);
    }
}

static void show_info(void) {
    char b[200];
    app_state_t st;
    state_lock(); st = g_state; state_unlock();
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(b, sizeof b,
             "WiFi:  %s\nIP:  %s\nTín hiệu:  %d dBm\nVị trí:  %s\nMAC:  %02X:%02X:%02X:%02X:%02X:%02X\n"
             "Firmware:  %s\nRAM trống:  %u KB",
             st.ssid[0] ? st.ssid : "--", st.ip[0] ? st.ip : "--", st.rssi, st.city[0] ? st.city : "--",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], FW_VERSION,
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_8BIT) / 1024));
    s_info = ui_box(s_scr, 12, 34, 296, 180, C_CARD, 12);
    lv_obj_set_style_border_color(s_info, lv_color_hex(C_ACCENT), 0);
    lv_obj_set_style_border_width(s_info, 1, 0);
    lv_obj_t *l = ui_label(s_info, &font_vi_15, C_TEXT, b);
    lv_obj_set_pos(l, 12, 8);
    lv_obj_set_style_text_line_space(l, 4, 0);
}

static void create(lv_obj_t *scr) {
    s_scr = scr;
    s_info = NULL;
    s_confirm = false;
    ui_title(scr, "Cài đặt");
    for (int i = 0; i < NITEMS; i++) {
        rows[i] = ui_box(scr, 12, 34 + i * 36, 296, 32, C_CARD, 10);
        lv_obj_t *n = ui_label(rows[i], &font_vi_15, C_TEXT, NAMES[i]);
        lv_obj_align(n, LV_ALIGN_LEFT_MID, 12, 0);
        vals[i] = ui_label(rows[i], &font_vi_15, C_ACCENT, "");
        lv_obj_align(vals[i], LV_ALIGN_RIGHT_MID, -12, 0);
    }
    refresh_values();
    ui_hint(scr, "Nhấn: mục tiếp · Giữ: đổi · Giữ lâu: quay lại");
}

static void destroy(void) { s_scr = s_info = NULL; }

static bool on_btn(btn_evt_t e) {
    if (s_info) {   // any press closes the info card
        lv_obj_delete(s_info);
        s_info = NULL;
        return true;
    }
    if (e == BTN_SHORT) {
        s_focus = (s_focus + 1) % NITEMS;
        s_confirm = false;
        refresh_values();
        return true;
    }
    if (e != BTN_LONG) return false;
    switch (s_focus) {
        case 0: g_cfg.h12 ^= 1; settings_save(); break;
        case 1: g_cfg.auto_home = (g_cfg.auto_home + 1) % 4; settings_save(); break;
        case 2: g_cfg.rot ^= 1; settings_save(); ui_apply_rotation(); break;
        case 3:
            if (s_confirm) net_forget_wifi();   // restarts into the setup portal
            s_confirm = true;
            break;
        case 4: show_info(); return true;
    }
    if (s_focus != 3) s_confirm = false;
    refresh_values();
    return true;
}

const screen_t APP_SETTINGS = {.create = create, .destroy = destroy, .on_btn = on_btn};
