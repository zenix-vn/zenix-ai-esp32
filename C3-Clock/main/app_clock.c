// Clock app: page 0 analog clock, page 1 stopwatch, page 2 countdown timer.
// Stopwatch/timer state lives here (not in the screen) so it keeps running in the background.
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "esp_timer.h"
#include "ui.h"

static int64_t now_us(void) { return esp_timer_get_time(); }

/* ---------------- background state ---------------- */
static bool sw_running;
static int64_t sw_start, sw_accum;          // us

typedef enum { T_IDLE, T_PICK, T_RUN, T_PAUSE, T_DONE } tstate_t;
static tstate_t t_state;
static const int PRESETS[] = {1, 3, 5, 10, 15, 25, 30, 45, 60};   // minutes
#define NPRE (int)(sizeof PRESETS / sizeof PRESETS[0])
static int t_preset = 2;
static int64_t t_end, t_left;               // us

static int64_t sw_elapsed(void) { return sw_accum + (sw_running ? now_us() - sw_start : 0); }
static int64_t t_total(void) { return PRESETS[t_preset] * 60LL * 1000000; }
static int64_t t_remaining(void) {
    if (t_state == T_RUN) { int64_t r = t_end - now_us(); return r > 0 ? r : 0; }
    if (t_state == T_PAUSE) return t_left;
    return t_total();
}

bool clock_timer_poll(void) {
    if (t_state == T_RUN && now_us() >= t_end) { t_state = T_DONE; return true; }
    return false;
}

void clock_timer_ack(void) { if (t_state == T_DONE) t_state = T_IDLE; }

bool clock_status(char *b, size_t n) {
    if (t_state == T_RUN || t_state == T_PAUSE) {
        int s = (int)((t_remaining() + 999999) / 1000000);
        snprintf(b, n, "Hẹn %02d:%02d", s / 60, s % 60);
        return true;
    }
    if (sw_running) {
        int s = (int)(sw_elapsed() / 1000000);
        snprintf(b, n, "Bấm %02d:%02d", (s / 60) % 100, s % 60);
        return true;
    }
    return false;
}

/* ---------------- screen ---------------- */
static lv_obj_t *s_scr, *s_title, *s_body, *s_dots, *s_hint;
static int s_page;
// analog
static lv_obj_t *a_scale, *a_h, *a_m, *a_s, *a_time, *a_sec, *a_date;
// stopwatch
static lv_obj_t *w_dig[4], *w_cs, *w_state;
// timer
static lv_obj_t *c_arc, *c_time, *c_state;

static const char *TITLES[] = {"Đồng hồ", "Bấm giờ", "Hẹn giờ"};

static lv_obj_t *needle(lv_obj_t *scale, int w, uint32_t col) {
    lv_obj_t *l = lv_line_create(scale);
    lv_obj_set_style_line_width(l, w, 0);
    lv_obj_set_style_line_color(l, lv_color_hex(col), 0);
    lv_obj_set_style_line_rounded(l, true, 0);
    return l;
}

static void build_analog(void) {
    a_scale = lv_scale_create(s_body);
    lv_obj_set_size(a_scale, 176, 176);
    lv_obj_set_pos(a_scale, 14, 32);
    lv_scale_set_mode(a_scale, LV_SCALE_MODE_ROUND_INNER);
    lv_scale_set_range(a_scale, 0, 3600);
    lv_scale_set_total_tick_count(a_scale, 61);
    lv_scale_set_major_tick_every(a_scale, 5);
    lv_scale_set_angle_range(a_scale, 360);
    lv_scale_set_rotation(a_scale, 270);
    lv_scale_set_label_show(a_scale, false);
    lv_obj_set_style_bg_color(a_scale, lv_color_hex(C_CARD), 0);
    lv_obj_set_style_bg_opa(a_scale, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(a_scale, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_color(a_scale, lv_color_hex(C_BORDER), 0);
    lv_obj_set_style_border_width(a_scale, 2, 0);
    lv_obj_set_style_pad_all(a_scale, 8, 0);
    lv_obj_set_style_arc_width(a_scale, 0, LV_PART_MAIN);
    lv_obj_set_style_length(a_scale, 5, LV_PART_ITEMS);
    lv_obj_set_style_line_width(a_scale, 1, LV_PART_ITEMS);
    lv_obj_set_style_line_color(a_scale, lv_color_hex(C_DIM), LV_PART_ITEMS);
    lv_obj_set_style_length(a_scale, 10, LV_PART_INDICATOR);
    lv_obj_set_style_line_width(a_scale, 3, LV_PART_INDICATOR);
    lv_obj_set_style_line_color(a_scale, lv_color_hex(C_TEXT), LV_PART_INDICATOR);
    a_h = needle(a_scale, 6, C_TEXT);
    a_m = needle(a_scale, 4, 0xCBD5E1);
    a_s = needle(a_scale, 2, C_ACCENT);
    lv_obj_t *dot = ui_box(a_scale, 0, 0, 12, 12, C_ACCENT, 6);
    lv_obj_center(dot);

    a_time = ui_label(s_body, &font_temp_44, C_TEXT, "--:--");
    lv_obj_set_pos(a_time, 202, 56);
    a_sec = ui_label(s_body, &font_vi_20, C_ACCENT, "");
    lv_obj_set_pos(a_sec, 204, 100);
    a_date = ui_label(s_body, &font_vi_15, C_SUB, "");
    lv_obj_set_pos(a_date, 204, 134);
    lv_obj_set_width(a_date, 110);
    lv_label_set_long_mode(a_date, LV_LABEL_LONG_WRAP);
}

static void build_stopwatch(void) {
    for (int i = 0; i < 4; i++) {
        w_dig[i] = ui_label(s_body, &font_clock_92, C_TEXT, "0");
        lv_obj_set_width(w_dig[i], 62);
        lv_obj_set_style_text_align(w_dig[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(w_dig[i], 18 + i * 62 + (i >= 2 ? 26 : 0), 50);
    }
    lv_obj_t *col = ui_label(s_body, &font_clock_92, C_ACCENT, ":");
    lv_obj_set_width(col, 26);
    lv_obj_set_style_text_align(col, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(col, 18 + 124, 46);
    w_cs = ui_label(s_body, &font_vi_20, C_ACCENT, ".00");
    lv_obj_align(w_cs, LV_ALIGN_TOP_RIGHT, -10, 100);
    w_state = ui_label(s_body, &font_vi_20, C_SUB, "");
    lv_obj_align(w_state, LV_ALIGN_TOP_MID, 0, 150);
}

static void build_timer(void) {
    c_arc = lv_arc_create(s_body);
    lv_obj_set_size(c_arc, 172, 172);
    lv_obj_align(c_arc, LV_ALIGN_TOP_MID, 0, 32);
    lv_arc_set_rotation(c_arc, 270);
    lv_arc_set_bg_angles(c_arc, 0, 360);
    lv_arc_set_range(c_arc, 0, 1000);
    lv_obj_remove_style(c_arc, NULL, LV_PART_KNOB);
    lv_obj_remove_flag(c_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(c_arc, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_color(c_arc, lv_color_hex(C_BORDER), LV_PART_MAIN);
    lv_obj_set_style_arc_width(c_arc, 10, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(c_arc, lv_color_hex(C_ACCENT), LV_PART_INDICATOR);
    c_time = ui_label(s_body, &font_temp_44, C_TEXT, "");
    lv_obj_align(c_time, LV_ALIGN_TOP_MID, 0, 92);
    c_state = ui_label(s_body, &font_vi_15, C_SUB, "");
    lv_obj_align(c_state, LV_ALIGN_TOP_MID, 0, 138);
}

static const char *HINTS[] = {"Nhấn: trang sau · Giữ lâu: quay lại", "", ""};

static void update_hint(void) {
    const char *h = HINTS[0];
    if (s_page == 1) h = sw_running ? "Giữ: dừng · Nhấn: trang sau"
                       : sw_accum ? "Giữ: đặt lại · Nhấn: trang sau" : "Giữ: bắt đầu · Nhấn: trang sau";
    if (s_page == 2) {
        switch (t_state) {
            case T_PICK: h = "Nhấn: đổi thời gian · Giữ: bắt đầu"; break;
            case T_RUN: h = "Giữ: tạm dừng · Nhấn: trang sau"; break;
            case T_PAUSE: h = "Giữ: tiếp tục · Nhấn: đặt lại"; break;
            default: h = "Giữ: chọn thời gian · Nhấn: trang sau"; break;
        }
    }
    lv_label_set_text(s_hint, h);
}

static void render(void) {
    lv_obj_clean(s_body);
    lv_label_set_text(s_title, TITLES[s_page]);
    ui_dots_set(s_dots, s_page);
    if (s_page == 0) build_analog();
    else if (s_page == 1) build_stopwatch();
    else build_timer();
    update_hint();
}

static void tick(void) {
    char b[40];
    if (s_page == 0) {
        time_t now = time(NULL);
        struct tm tm;
        localtime_r(&now, &tm);
        lv_scale_set_line_needle_value(a_scale, a_h, 46, ((tm.tm_hour % 12) * 60 + tm.tm_min) * 5);
        lv_scale_set_line_needle_value(a_scale, a_m, 66, tm.tm_min * 60 + tm.tm_sec);
        lv_scale_set_line_needle_value(a_scale, a_s, 72, tm.tm_sec * 60);
        ui_fmt_hm(b, sizeof b, &tm);
        if (g_cfg.h12) b[strlen(b) - 3] = 0;   // drop " SA/CH" to fit the big font
        lv_label_set_text(a_time, g_state.time_synced ? b : "--:--");
        snprintf(b, sizeof b, ":%02d %s", tm.tm_sec, g_cfg.h12 ? (tm.tm_hour < 12 ? "SA" : "CH") : "");
        lv_label_set_text(a_sec, b);
        static const char *WD[] = {"Chủ Nhật", "Thứ Hai", "Thứ Ba", "Thứ Tư", "Thứ Năm", "Thứ Sáu", "Thứ Bảy"};
        snprintf(b, sizeof b, "%s\n%02d/%02d/%d", WD[tm.tm_wday], tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900);
        lv_label_set_text(a_date, b);
    } else if (s_page == 1) {
        int64_t e = sw_elapsed();
        int cs = (int)(e / 10000 % 100), s = (int)(e / 1000000);
        int a, c;
        if (s < 3600) { a = s / 60; c = s % 60; snprintf(b, sizeof b, ".%02d", cs); }
        else { a = s / 3600; c = (s / 60) % 60; snprintf(b, sizeof b, ":%02d", s % 60); }
        char d[8];
        snprintf(d, sizeof d, "%02d%02d", a % 100, c);
        for (int i = 0; i < 4; i++) { char t[2] = {d[i], 0}; lv_label_set_text(w_dig[i], t); }
        lv_label_set_text(w_cs, b);
        lv_label_set_text(w_state, sw_running ? "Đang chạy" : sw_accum ? "Tạm dừng" : "Sẵn sàng");
        lv_obj_set_style_text_color(w_state, lv_color_hex(sw_running ? C_OK : C_SUB), 0);
    } else {
        int64_t r = t_remaining();
        int s = (int)((r + 999999) / 1000000);
        snprintf(b, sizeof b, "%02d:%02d", s / 60, s % 60);
        lv_label_set_text(c_time, b);
        bool blink = t_state == T_PICK && (esp_timer_get_time() / 400000) & 1;
        lv_obj_set_style_text_color(c_time, lv_color_hex(t_state == T_PICK ? (blink ? C_DIM : C_ACCENT) : C_TEXT), 0);
        lv_arc_set_value(c_arc, (int)(r * 1000 / t_total()));
        const char *st = t_state == T_RUN ? "Đang đếm ngược" : t_state == T_PAUSE ? "Tạm dừng"
                       : t_state == T_PICK ? "Chọn thời gian" : "Sẵn sàng";
        lv_label_set_text(c_state, st);
        lv_obj_set_style_arc_color(c_arc, lv_color_hex(t_state == T_PAUSE ? C_WARN : C_ACCENT), LV_PART_INDICATOR);
    }
}

static void next_page(void) { s_page = (s_page + 1) % 3; render(); tick(); }

static bool on_btn(btn_evt_t e) {
    if (s_page == 1 && e == BTN_LONG) {
        if (sw_running) { sw_accum += now_us() - sw_start; sw_running = false; }
        else if (sw_accum) sw_accum = 0;
        else { sw_start = now_us(); sw_running = true; }
        update_hint(); tick();
        return true;
    }
    if (s_page == 2) {
        switch (t_state) {
            case T_IDLE:
            case T_DONE:
                if (e == BTN_LONG) { t_state = T_PICK; update_hint(); tick(); return true; }
                break;
            case T_PICK:
                if (e == BTN_SHORT) { t_preset = (t_preset + 1) % NPRE; tick(); return true; }
                if (e == BTN_LONG) { t_end = now_us() + t_total(); t_state = T_RUN; update_hint(); tick(); return true; }
                if (e == BTN_HOLD) { t_state = T_IDLE; update_hint(); tick(); return true; }
                break;
            case T_RUN:
                if (e == BTN_LONG) { t_left = t_end - now_us(); t_state = T_PAUSE; update_hint(); tick(); return true; }
                break;
            case T_PAUSE:
                if (e == BTN_LONG) { t_end = now_us() + t_left; t_state = T_RUN; update_hint(); tick(); return true; }
                if (e == BTN_SHORT) { t_state = T_IDLE; update_hint(); tick(); return true; }
                break;
        }
    }
    if (e == BTN_SHORT) { next_page(); return true; }
    if (e == BTN_LONG) return true;
    return false;
}

static void create(lv_obj_t *scr) {
    s_scr = scr;
    s_title = ui_title(scr, TITLES[0]);
    s_body = lv_obj_create(scr);
    lv_obj_remove_style_all(s_body);
    lv_obj_set_size(s_body, SCR_W, SCR_H - 20);
    lv_obj_remove_flag(s_body, LV_OBJ_FLAG_SCROLLABLE);
    s_dots = ui_dots(scr, 3);
    s_hint = ui_label(scr, &font_vi_15, C_DIM, "");
    lv_obj_align(s_hint, LV_ALIGN_BOTTOM_LEFT, 12, -3);
    // jump straight to whatever is running
    s_page = (t_state == T_RUN || t_state == T_PAUSE) ? 2 : sw_running ? 1 : 0;
    render();
}

static void destroy(void) { s_scr = s_body = NULL; }

const screen_t APP_CLOCK = {.create = create, .destroy = destroy, .tick = tick, .on_btn = on_btn, .no_auto_home = true};
