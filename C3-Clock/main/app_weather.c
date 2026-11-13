// Weather app: page 0 now, page 1 next 24 h, page 2 next 7 days.
// Short = next page, long = refresh now, hold = back.
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "ui.h"

static lv_obj_t *s_scr, *s_title, *s_body, *s_dots;
static int s_page;
static int64_t s_shown_update = -1;
static const char *TITLES[] = {"Thời tiết", "24 giờ tới", "7 ngày tới"};

static int rnd(float v) { return (int)lroundf(v); }

static lv_obj_t *stat_tile(lv_obj_t *p, int x, int y, const char *cap, const char *val, uint32_t vcol) {
    lv_obj_t *t = ui_box(p, x, y, 96, 46, C_CARD, 10);
    lv_obj_t *c = ui_label(t, &font_vi_15, C_SUB, cap);
    lv_obj_set_pos(c, 8, 3);
    lv_obj_t *v = ui_label(t, &font_vi_20, vcol, val);
    lv_obj_set_pos(v, 8, 20);
    return t;
}

static void fmt_clock(char *b, size_t n, int64_t t) {
    if (t <= 0) { snprintf(b, n, "--:--"); return; }
    time_t tt = (time_t)t;
    struct tm tm;
    localtime_r(&tt, &tm);
    snprintf(b, n, "%02d:%02d", tm.tm_hour, tm.tm_min);
}

static void page_now(const app_state_t *st) {
    const char *icon, *desc;
    uint32_t col;
    char b[48];
    weather_look(st->code, st->is_day, &icon, &col, &desc);
    lv_obj_t *i = ui_label(s_body, &font_wx_48, col, icon);
    lv_obj_set_pos(i, 14, 34);
    snprintf(b, sizeof b, "%d°", rnd(st->temp));
    lv_obj_t *t = ui_label(s_body, &font_temp_44, C_TEXT, b);
    lv_obj_set_pos(t, 76, 36);
    lv_obj_t *d = ui_label(s_body, &font_vi_15, C_TEXT, desc);
    lv_obj_set_pos(d, 78, 74);

    snprintf(b, sizeof b, "Cảm giác %d°", rnd(st->feels));
    lv_obj_t *f = ui_label(s_body, &font_vi_15, C_SUB, b);
    lv_obj_align(f, LV_ALIGN_TOP_RIGHT, -14, 40);
    snprintf(b, sizeof b, "Cao %d° · Thấp %d°", rnd(st->tmax), rnd(st->tmin));
    lv_obj_t *hl = ui_label(s_body, &font_vi_15, C_SUB, b);
    lv_obj_align(hl, LV_ALIGN_TOP_RIGHT, -14, 60);
    if (st->city[0]) {
        lv_obj_t *c = ui_label(s_body, &font_vi_15, C_DIM, st->city);
        lv_obj_align(c, LV_ALIGN_TOP_RIGHT, -14, 80);
    }

    snprintf(b, sizeof b, "%d%%", st->humidity);
    stat_tile(s_body, 12, 104, "Độ ẩm", b, C_TEXT);
    snprintf(b, sizeof b, "%d km/h", rnd(st->wind));
    stat_tile(s_body, 112, 104, "Gió", b, C_TEXT);
    float uv = st->uv;
    const char *uvt = uv < 3 ? "Thấp" : uv < 6 ? "TB" : uv < 8 ? "Cao" : uv < 11 ? "Rất cao" : "Cực cao";
    uint32_t uvc = uv < 3 ? C_OK : uv < 6 ? C_WARN : uv < 8 ? 0xFB923C : C_BAD;
    snprintf(b, sizeof b, "%d %s", rnd(uv), uvt);
    stat_tile(s_body, 212, 104, "Chỉ số UV", b, uvc);
    if (st->aq_valid) {
        int a = st->aqi;
        const char *at = a <= 50 ? "Tốt" : a <= 100 ? "TB" : a <= 150 ? "Kém" : a <= 200 ? "Xấu" : "Rất xấu";
        uint32_t ac = a <= 50 ? C_OK : a <= 100 ? C_WARN : a <= 150 ? 0xFB923C : a <= 200 ? C_BAD : 0xC084FC;
        snprintf(b, sizeof b, "%d %s", a, at);
        stat_tile(s_body, 12, 156, "Không khí", b, ac);
    } else {
        stat_tile(s_body, 12, 156, "Không khí", "--", C_SUB);
    }
    fmt_clock(b, sizeof b, st->sunrise);
    stat_tile(s_body, 112, 156, "Bình minh", b, 0xFCD34D);
    fmt_clock(b, sizeof b, st->sunset);
    stat_tile(s_body, 212, 156, "Hoàng hôn", b, 0xFB923C);
}

static void page_hours(const app_state_t *st) {
    char b[16];
    for (int i = 0; i < st->nhours && i < 8; i++) {
        const wx_hour_t *h = &st->hours[i];
        int x = 8 + i * 38;
        time_t tt = (time_t)h->t;
        struct tm tm;
        localtime_r(&tt, &tm);
        if (i == 0) snprintf(b, sizeof b, "Nay");
        else snprintf(b, sizeof b, "%dh", tm.tm_hour);
        lv_obj_t *l = ui_label(s_body, &font_vi_15, i == 0 ? C_ACCENT : C_SUB, b);
        lv_obj_set_width(l, 38);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(l, x, 36);

        const char *icon, *desc;
        uint32_t col;
        bool day = tm.tm_hour >= 6 && tm.tm_hour < 18;
        weather_look(h->code, day, &icon, &col, &desc);
        lv_obj_t *ic = ui_label(s_body, &font_wx_24, col, icon);
        lv_obj_set_width(ic, 38);
        lv_obj_set_style_text_align(ic, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(ic, x, 60);

        snprintf(b, sizeof b, "%d°", rnd(h->temp));
        lv_obj_t *t = ui_label(s_body, &font_vi_20, C_TEXT, b);
        lv_obj_set_width(t, 38);
        lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(t, x, 92);

        int pop = h->pop < 0 ? 0 : h->pop > 100 ? 100 : h->pop;
        ui_box(s_body, x + 15, 124, 8, 56, C_BORDER, 4);
        int bh = pop * 56 / 100;
        if (bh > 0) ui_box(s_body, x + 15, 124 + 56 - bh, 8, bh, 0x3B82F6, 4);
        snprintf(b, sizeof b, "%d%%", pop);
        lv_obj_t *p = ui_label(s_body, &font_vi_15, 0x7DD3FC, b);
        lv_obj_set_width(p, 38);
        lv_obj_set_style_text_align(p, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(p, x, 186);
    }
    if (st->nhours == 0) {
        lv_obj_t *l = ui_label(s_body, &font_vi_15, C_SUB, "Chưa có dữ liệu");
        lv_obj_center(l);
    }
}

static void page_days(const app_state_t *st) {
    static const char *WD[] = {"CN", "T2", "T3", "T4", "T5", "T6", "T7"};
    char b[24];
    float lo = 99, hi = -99;
    for (int i = 0; i < st->ndays; i++) {
        if (st->days[i].tmin < lo) lo = st->days[i].tmin;
        if (st->days[i].tmax > hi) hi = st->days[i].tmax;
    }
    float span = hi - lo < 1 ? 1 : hi - lo;
    for (int i = 0; i < st->ndays && i < 7; i++) {
        const wx_day_t *d = &st->days[i];
        int y = 33 + i * 26;
        time_t tt = (time_t)d->t;
        struct tm tm;
        localtime_r(&tt, &tm);
        if (i == 0) snprintf(b, sizeof b, "Hôm nay");
        else snprintf(b, sizeof b, "%s %d/%d", WD[tm.tm_wday], tm.tm_mday, tm.tm_mon + 1);
        lv_obj_t *l = ui_label(s_body, &font_vi_15, i == 0 ? C_ACCENT : C_TEXT, b);
        lv_obj_set_pos(l, 12, y + 4);

        const char *icon, *desc;
        uint32_t col;
        weather_look(d->code, true, &icon, &col, &desc);
        lv_obj_t *ic = ui_label(s_body, &font_wx_24, col, icon);
        lv_obj_set_pos(ic, 96, y);

        lv_obj_t *dr = ui_label(s_body, &font_ic_16, 0x60A5FA, IC_DROP);
        lv_obj_set_pos(dr, 136, y + 5);
        snprintf(b, sizeof b, "%d%%", d->pop);
        lv_obj_t *p = ui_label(s_body, &font_vi_15, 0x7DD3FC, b);
        lv_obj_set_pos(p, 150, y + 4);

        snprintf(b, sizeof b, "%d°", rnd(d->tmin));
        lv_obj_t *mn = ui_label(s_body, &font_vi_15, C_SUB, b);
        lv_obj_set_width(mn, 32);
        lv_obj_set_style_text_align(mn, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_pos(mn, 192, y + 4);
        // temperature range bar, scaled to the week's min..max
        ui_box(s_body, 230, y + 11, 50, 4, C_BORDER, 2);
        int x0 = 230 + (int)((d->tmin - lo) / span * 50), x1 = 230 + (int)((d->tmax - lo) / span * 50);
        lv_obj_t *rb = ui_box(s_body, x0, y + 11, x1 - x0 < 4 ? 4 : x1 - x0, 4, 0xFB923C, 2);
        lv_obj_set_style_bg_grad_color(rb, lv_color_hex(0x60A5FA), 0);
        lv_obj_set_style_bg_grad_dir(rb, LV_GRAD_DIR_HOR, 0);
        lv_obj_set_style_bg_color(rb, lv_color_hex(0x60A5FA), 0);
        lv_obj_set_style_bg_grad_color(rb, lv_color_hex(0xFB923C), 0);
        snprintf(b, sizeof b, "%d°", rnd(d->tmax));
        lv_obj_t *mx = ui_label(s_body, &font_vi_15, C_TEXT, b);
        lv_obj_set_pos(mx, 286, y + 4);
    }
    if (st->ndays == 0) {
        lv_obj_t *l = ui_label(s_body, &font_vi_15, C_SUB, "Chưa có dữ liệu");
        lv_obj_center(l);
    }
}

static void render(void) {
    app_state_t st;
    state_lock(); st = g_state; state_unlock();
    s_shown_update = st.wx_updated;
    lv_obj_clean(s_body);
    lv_label_set_text(s_title, TITLES[s_page]);
    ui_dots_set(s_dots, s_page);
    if (!st.wx_valid) {
        lv_obj_t *l = ui_label(s_body, &font_vi_15, C_SUB,
                               st.wifi == WIFI_ST_CONNECTED ? "Đang tải thời tiết..." : "Chưa kết nối WiFi");
        lv_obj_center(l);
        return;
    }
    if (s_page == 0) page_now(&st);
    else if (s_page == 1) page_hours(&st);
    else page_days(&st);
}

static void create(lv_obj_t *scr) {
    s_scr = scr;
    s_title = ui_title(scr, TITLES[0]);
    s_body = lv_obj_create(scr);
    lv_obj_remove_style_all(s_body);
    lv_obj_set_size(s_body, SCR_W, SCR_H - 20);
    lv_obj_remove_flag(s_body, LV_OBJ_FLAG_SCROLLABLE);
    s_dots = ui_dots(scr, 3);
    ui_hint(scr, "Nhấn: trang sau · Giữ: cập nhật");
    s_page = 0;
    render();
}

static void destroy(void) { s_scr = s_body = s_title = s_dots = NULL; }

static void tick(void) {
    if (g_state.wx_updated != s_shown_update) render();
}

static bool on_btn(btn_evt_t e) {
    if (e == BTN_SHORT) { s_page = (s_page + 1) % 3; render(); return true; }
    if (e == BTN_LONG) { net_refresh(); return true; }
    return false;
}

const screen_t APP_WEATHER = {.create = create, .destroy = destroy, .tick = tick, .on_btn = on_btn};
