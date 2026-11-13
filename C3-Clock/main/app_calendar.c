// Calendar app with two views.
//   Day view  (tear-off "lịch bloc" style): short = next day,  long = month view
//   Month view (full-width grid):           short = next week, long = day view
// Hold = back to the app screen; re-opening the app starts again at today.
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "lunar.h"
#include "ui.h"

#define C_RED   0xEF4444
#define C_LUNAR 0xFB923C

static lv_obj_t *s_title, *s_body, *s_hint;
static bool s_month_view;
static int s_d, s_m, s_y;   // selected date
static int t_d, t_m, t_y;   // today

static const char *WD[] = {"Chủ Nhật", "Thứ Hai", "Thứ Ba", "Thứ Tư", "Thứ Năm", "Thứ Sáu", "Thứ Bảy"};

static bool is_red_day(int d, int m, int y, const lunar_t *l) {
    const char *ev = lunar_holiday(d, m, l);
    bool real_holiday = ev && l->day != 1 && l->day != 15;
    return solar_weekday(d, m, y) == 0 || real_holiday;
}

static lv_obj_t *centered(lv_obj_t *p, const lv_font_t *f, uint32_t col, const char *txt, int y, int w) {
    lv_obj_t *l = ui_label(p, f, col, txt);
    lv_obj_set_width(l, w);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(l, 0, y);
    return l;
}

static lv_obj_t *card(lv_obj_t *p, int x, int y, int w, int h) {
    lv_obj_t *c = ui_box(p, x, y, w, h, C_CARD, 16);
    lv_obj_set_style_border_color(c, lv_color_hex(C_BORDER), 0);
    lv_obj_set_style_border_width(c, 1, 0);
    lv_obj_set_style_clip_corner(c, true, 0);
    return c;
}

/* ---------------- day view ---------------- */
static void build_day(void) {
    char b[80], cc[24];
    lunar_t l;
    lunar_from_solar(s_d, s_m, s_y, &l);
    bool red = is_red_day(s_d, s_m, s_y, &l);
    bool today = s_d == t_d && s_m == t_m && s_y == t_y;

    // left: tear-off sheet
    lv_obj_t *sheet = card(s_body, 12, 34, 176, 180);
    lv_obj_t *band = ui_box(sheet, 0, 0, 176, 30, red ? 0xDC2626 : 0x2563EB, 0);
    snprintf(b, sizeof b, "%s%s", WD[solar_weekday(s_d, s_m, s_y)], today ? " · Hôm nay" : "");
    lv_obj_t *wd = centered(band, &font_vi_15, 0xFFFFFF, b, 6, 176);
    (void)wd;
    snprintf(b, sizeof b, "%d", s_d);
    centered(sheet, &font_clock_92, red ? C_RED : C_TEXT, b, 38, 176);
    const char *ev = lunar_holiday(s_d, s_m, &l);
    if (ev) centered(sheet, &font_vi_15, C_WARN, ev, 110, 176);
    else {
        snprintf(b, sizeof b, "Tiết %s", lunar_solar_term(s_d, s_m, s_y));
        centered(sheet, &font_vi_15, C_SUB, b, 110, 176);
    }
    ui_box(sheet, 14, 136, 148, 1, C_BORDER, 0);
    lv_obj_t *gc = ui_label(sheet, &font_vi_15, C_DIM, "Giờ hoàng đạo");
    lv_obj_set_pos(gc, 12, 142);
    char gh[48];
    lunar_good_hours(l.jd, gh, sizeof gh);
    lv_obj_t *gl = ui_label(sheet, &font_vi_15, C_TEXT, gh);
    lv_obj_set_pos(gl, 12, 159);
    lv_obj_set_width(gl, 156);
    lv_label_set_long_mode(gl, LV_LABEL_LONG_SCROLL_CIRCULAR);

    // right: lunar date
    lv_obj_t *lc = card(s_body, 196, 34, 112, 180);
    centered(lc, &font_vi_15, C_DIM, "ÂM LỊCH", 8, 112);
    snprintf(b, sizeof b, "%d", l.day);
    centered(lc, &font_temp_44, C_LUNAR, b, 28, 112);
    snprintf(b, sizeof b, "Tháng %d%s", l.month, l.leap ? " nhuận" : "");
    centered(lc, &font_vi_15, C_TEXT, b, 66, 112);
    ui_box(lc, 14, 94, 84, 1, C_BORDER, 0);
    lunar_can_chi_day(l.jd, cc, sizeof cc);
    snprintf(b, sizeof b, "Ngày %s", cc);
    centered(lc, &font_vi_15, C_SUB, b, 104, 112);
    lunar_can_chi_month(l.month, l.year, cc, sizeof cc);
    snprintf(b, sizeof b, "Tháng %s", cc);
    centered(lc, &font_vi_15, C_SUB, b, 126, 112);
    lunar_can_chi_year(l.year, cc, sizeof cc);
    snprintf(b, sizeof b, "Năm %s", cc);
    centered(lc, &font_vi_15, C_TEXT, b, 148, 112);
}

/* ---------------- month view ---------------- */
#define MG_X 6
#define MG_Y 50
#define MC_W 44
#define MC_H 27

static void build_month(void) {
    static const char *H[] = {"T2", "T3", "T4", "T5", "T6", "T7", "CN"};
    char b[12];
    for (int c = 0; c < 7; c++) {
        lv_obj_t *h = ui_label(s_body, &font_vi_15, c == 6 ? C_RED : C_DIM, H[c]);
        lv_obj_set_width(h, MC_W);
        lv_obj_set_style_text_align(h, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(h, MG_X + c * MC_W, 30);
    }
    int fc = (solar_weekday(1, s_m, s_y) + 6) % 7, nd = solar_days_in_month(s_m, s_y);
    for (int day = 1; day <= nd; day++) {
        int i = fc + day - 1, x = MG_X + (i % 7) * MC_W, y = MG_Y + (i / 7) * MC_H;
        lunar_t l;
        lunar_from_solar(day, s_m, s_y, &l);
        bool sel = day == s_d, today = day == t_d && s_m == t_m && s_y == t_y;
        if (sel) ui_box(s_body, x + 3, y, MC_W - 6, MC_H - 1, C_ACCENT, 8);
        else if (today) {
            lv_obj_t *r = ui_box(s_body, x + 3, y, MC_W - 6, MC_H - 1, C_BG_TOP, 8);
            lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_color(r, lv_color_hex(C_ACCENT), 0);
            lv_obj_set_style_border_width(r, 1, 0);
        }
        uint32_t sc = sel ? 0x04121F : is_red_day(day, s_m, s_y, &l) ? C_RED : C_TEXT;
        snprintf(b, sizeof b, "%d", day);
        lv_obj_t *s = ui_label(s_body, &font_vi_15, sc, b);
        lv_obj_set_width(s, MC_W);
        lv_obj_set_style_text_align(s, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(s, x, y);
        if (l.day == 1) snprintf(b, sizeof b, "%d/%d", l.day, l.month);
        else snprintf(b, sizeof b, "%d", l.day);
        uint32_t lcol = sel ? 0x0B3B5C : (l.day == 1 || l.day == 15) ? C_LUNAR : C_DIM;
        lv_obj_t *lu = ui_label(s_body, &font_tiny_11, lcol, b);
        lv_obj_set_width(lu, MC_W);
        lv_obj_set_style_text_align(lu, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(lu, x, y + 15);
    }
}

/* ---------------- common ---------------- */
static void render(void) {
    char b[32];
    snprintf(b, sizeof b, "Tháng %d · %d", s_m, s_y);
    lv_label_set_text(s_title, b);
    lv_obj_clean(s_body);
    if (s_month_view) build_month();
    else build_day();
    lv_label_set_text(s_hint, s_month_view ? "Nhấn: tuần sau · Giữ: xem ngày" : "Nhấn: ngày sau · Giữ: xem tháng");
}

static void add_days(int n) {
    while (n--) {
        if (++s_d > solar_days_in_month(s_m, s_y)) {
            s_d = 1;
            if (++s_m > 12) { s_m = 1; s_y++; }
        }
    }
}

static void create(lv_obj_t *scr) {
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    if (tm.tm_year + 1900 < 2024) { tm.tm_mday = 1; tm.tm_mon = 0; tm.tm_year = 126; }   // clock not synced yet
    t_d = s_d = tm.tm_mday;
    t_m = s_m = tm.tm_mon + 1;
    t_y = s_y = tm.tm_year + 1900;
    s_month_view = false;

    s_title = ui_title(scr, "");
    s_body = lv_obj_create(scr);
    lv_obj_remove_style_all(s_body);
    lv_obj_set_size(s_body, SCR_W, SCR_H - 20);
    lv_obj_remove_flag(s_body, LV_OBJ_FLAG_SCROLLABLE);
    s_hint = ui_label(scr, &font_vi_15, C_DIM, "");
    lv_obj_align(s_hint, LV_ALIGN_BOTTOM_LEFT, 12, -3);
    render();
}

static void destroy(void) { s_title = s_body = s_hint = NULL; }

static bool on_btn(btn_evt_t e) {
    if (e == BTN_SHORT) { add_days(s_month_view ? 7 : 1); render(); return true; }
    if (e == BTN_LONG) { s_month_view = !s_month_view; render(); return true; }
    return false;
}

const screen_t APP_CAL = {.create = create, .destroy = destroy, .on_btn = on_btn};
