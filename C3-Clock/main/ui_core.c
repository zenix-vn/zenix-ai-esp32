// LVGL glue, one-button input, screen manager, shared widgets.
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "src/draw/sw/lv_draw_sw_utils.h"
#include "ui.h"

#define PIN_BOOT 9
#define LONG_MS  600    // release after this = "select"
#define HOLD_MS  1800   // keep holding this long = "back"

static const char *TAG = "ui";
static esp_lcd_panel_handle_t s_panel;
static lv_display_t *s_disp;
static volatile uint32_t s_flushes;
static const screen_t *s_cur;
static lv_obj_t *s_title_time;
static int64_t s_last_input_ms;
static lv_obj_t *s_hold_bar, *s_hold_tip, *s_alarm;

static size_t lv_mon_free(void) { lv_mem_monitor_t m; lv_mem_monitor(&m); return m.free_size; }

static int64_t now_ms(void) { return esp_timer_get_time() / 1000; }

/* ---------------- display glue ---------------- */
static void flush_done(void) { s_flushes++; lv_display_flush_ready(s_disp); }

static void flush_cb(lv_display_t *d, const lv_area_t *a, uint8_t *px) {
    lv_draw_sw_rgb565_swap(px, lv_area_get_size(a));
    esp_lcd_panel_draw_bitmap(s_panel, a->x1, a->y1, a->x2 + 1, a->y2 + 1, px);
}

static uint32_t tick_cb(void) { return (uint32_t)now_ms(); }

void ui_apply_rotation(void) {
    lcd_set_rotation(g_cfg.rot);
    lv_obj_invalidate(lv_screen_active());
}

/* ---------------- helpers ---------------- */
lv_obj_t *ui_label(lv_obj_t *parent, const lv_font_t *f, uint32_t color, const char *txt) {
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_label_set_text(l, txt);
    return l;
}

lv_obj_t *ui_box(lv_obj_t *parent, int x, int y, int w, int h, uint32_t bg, int radius) {
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, lv_color_hex(bg), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(o, radius, 0);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return o;
}

void ui_fmt_hm(char *buf, size_t n, const struct tm *tm) {
    if (g_cfg.h12) {
        int h = tm->tm_hour % 12;
        snprintf(buf, n, "%d:%02d %s", h ? h : 12, tm->tm_min, tm->tm_hour < 12 ? "SA" : "CH");
    } else {
        snprintf(buf, n, "%02d:%02d", tm->tm_hour, tm->tm_min);
    }
}

static void update_title_time(void) {
    if (!s_title_time) return;
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    char b[16];
    if (g_state.time_synced) ui_fmt_hm(b, sizeof b, &tm);
    else strcpy(b, "--:--");
    lv_label_set_text(s_title_time, b);
}

lv_obj_t *ui_title(lv_obj_t *scr, const char *title) {
    lv_obj_t *t = ui_label(scr, &font_vi_20, C_TEXT, title);
    lv_obj_set_pos(t, 14, 6);
    s_title_time = ui_label(scr, &font_vi_15, C_SUB, "");
    lv_obj_align(s_title_time, LV_ALIGN_TOP_RIGHT, -14, 10);
    update_title_time();
    return t;
}

void ui_hint(lv_obj_t *scr, const char *txt) {
    lv_obj_t *l = ui_label(scr, &font_vi_15, C_DIM, txt);
    lv_obj_align(l, LV_ALIGN_BOTTOM_LEFT, 12, -3);
}

lv_obj_t *ui_dots(lv_obj_t *scr, int n) {
    lv_obj_t *c = lv_obj_create(scr);
    lv_obj_remove_style_all(c);
    lv_obj_set_size(c, n * 12, 8);
    lv_obj_align(c, LV_ALIGN_BOTTOM_RIGHT, -12, -8);
    for (int i = 0; i < n; i++) ui_box(c, i * 12, 1, 6, 6, C_BORDER, 3);
    return c;
}

void ui_dots_set(lv_obj_t *dots, int idx) {
    for (uint32_t i = 0; i < lv_obj_get_child_count(dots); i++) {
        lv_obj_t *d = lv_obj_get_child(dots, i);
        lv_obj_set_style_bg_color(d, lv_color_hex(i == idx ? C_ACCENT : C_BORDER), 0);
        lv_obj_set_width(d, i == idx ? 10 : 6);
        lv_obj_set_x(d, i * 12 - (i > idx ? 0 : 0));
    }
}

void weather_look(int code, bool day, const char **icon, uint32_t *color, const char **desc) {
    if (code == 0)                     { *icon = day ? IC_SUN : IC_MOON;             *color = day ? 0xFBBF24 : 0xE2E8F0; *desc = "Trời quang"; }
    else if (code <= 2)                { *icon = day ? IC_CLOUD_SUN : IC_CLOUD_MOON; *color = day ? 0xFCD34D : 0xCBD5E1; *desc = code == 1 ? "Ít mây" : "Có mây"; }
    else if (code == 3)                { *icon = IC_CLOUD;   *color = 0xCBD5E1; *desc = "Nhiều mây"; }
    else if (code == 45 || code == 48) { *icon = IC_SMOG;    *color = 0x94A3B8; *desc = "Sương mù"; }
    else if (code >= 51 && code <= 57) { *icon = IC_RAIN;    *color = 0x7DD3FC; *desc = "Mưa phùn"; }
    else if (code >= 61 && code <= 67) { *icon = IC_RAIN;    *color = 0x60A5FA; *desc = code >= 65 ? "Mưa to" : "Mưa"; }
    else if (code >= 71 && code <= 77) { *icon = IC_SNOW;    *color = 0xE0F2FE; *desc = "Tuyết"; }
    else if (code >= 80 && code <= 82) { *icon = IC_SHOWERS; *color = 0x60A5FA; *desc = "Mưa rào"; }
    else if (code >= 85 && code <= 86) { *icon = IC_SNOW;    *color = 0xE0F2FE; *desc = "Tuyết rơi"; }
    else if (code >= 95)               { *icon = IC_BOLT;    *color = 0xFACC15; *desc = "Giông bão"; }
    else                               { *icon = IC_CLOUD;   *color = 0xCBD5E1; *desc = "--"; }
}

/* ---------------- screen manager ---------------- */
static void screen_bg(lv_obj_t *scr) {
    lv_obj_set_style_bg_color(scr, lv_color_hex(C_BG_TOP), 0);
    lv_obj_set_style_bg_grad_color(scr, lv_color_hex(C_BG_BOT), 0);
    lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
}

void ui_go(const screen_t *s, lv_screen_load_anim_t anim) {
    if (s_cur && s_cur->destroy) s_cur->destroy();
    s_title_time = NULL;
    lv_obj_t *scr = lv_obj_create(NULL);
    screen_bg(scr);
    s_cur = s;
    s->create(scr);
    if (s->tick) s->tick();
    lv_screen_load_anim(scr, anim, anim == LV_SCREEN_LOAD_ANIM_NONE ? 0 : 200, 0, true);
}

void ui_back(void) {
    if (s_cur == &SCR_HOME) return;
    if (s_cur == &SCR_APPS) ui_go(&SCR_HOME, LV_SCREEN_LOAD_ANIM_MOVE_RIGHT);
    else ui_go(&SCR_APPS, LV_SCREEN_LOAD_ANIM_MOVE_RIGHT);
}

/* ---------------- timer alarm overlay ---------------- */
static void alarm_build(void) {
    s_alarm = ui_box(lv_layer_top(), 0, 0, SCR_W, SCR_H, 0xB91C1C, 0);
    lv_obj_t *i = ui_label(s_alarm, &font_wx_48, C_TEXT, "");
    lv_obj_set_style_text_font(i, &font_ic_16, 0);
    lv_label_set_text(i, IC_HOURGLASS);
    lv_obj_align(i, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_t *t = ui_label(s_alarm, &font_clock_92, C_TEXT, "00:00");
    lv_obj_align(t, LV_ALIGN_CENTER, 0, -10);
    lv_obj_t *m = ui_label(s_alarm, &font_vi_20, C_TEXT, "Hết giờ!");
    lv_obj_align(m, LV_ALIGN_BOTTOM_MID, 0, -50);
    lv_obj_t *h = ui_label(s_alarm, &font_vi_15, 0xFECACA, "Nhấn nút để tắt");
    lv_obj_align(h, LV_ALIGN_BOTTOM_MID, 0, -24);
    lv_obj_add_flag(s_alarm, LV_OBJ_FLAG_HIDDEN);
}

/* ---------------- button ---------------- */
static void dispatch(btn_evt_t e) {
    s_last_input_ms = now_ms();
    if (!lv_obj_has_flag(s_alarm, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_add_flag(s_alarm, LV_OBJ_FLAG_HIDDEN);
        clock_timer_ack();
        return;
    }
    if (s_cur->on_btn && s_cur->on_btn(e)) return;
    if (e == BTN_HOLD) ui_back();
}

static void hold_feedback(int64_t d) {
    if (d < 150) {
        lv_obj_add_flag(s_hold_bar, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_hold_tip, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_remove_flag(s_hold_bar, LV_OBJ_FLAG_HIDDEN);
    int w = (int)(d * SCR_W / HOLD_MS);
    lv_obj_set_width(s_hold_bar, w > SCR_W ? SCR_W : w);
    lv_obj_set_style_bg_color(s_hold_bar, lv_color_hex(d < LONG_MS ? C_DIM : C_ACCENT), 0);
    if (d >= LONG_MS && s_cur != &SCR_HOME) {
        lv_obj_remove_flag(s_hold_tip, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_hold_tip, LV_OBJ_FLAG_HIDDEN);
    }
}

static void button_poll(void) {
    static bool last_raw, down;
    static int stable;
    static int64_t t0;
    static bool fired;
    bool raw = gpio_get_level(PIN_BOOT) == 0;
    if (raw != last_raw) { last_raw = raw; stable = 0; return; }
    if (stable < 2) { stable++; return; }      // ~20-30 ms debounce
    int64_t now = now_ms();
    if (raw && !down) { down = true; t0 = now; fired = false; }
    if (down && raw) {
        int64_t d = now - t0;
        if (!fired) hold_feedback(d);
        if (!fired && d >= HOLD_MS) {
            fired = true;
            hold_feedback(0);
            dispatch(BTN_HOLD);
        }
    }
    if (down && !raw) {
        down = false;
        hold_feedback(0);
        if (!fired) dispatch(now - t0 >= LONG_MS ? BTN_LONG : BTN_SHORT);
    }
}

/* ---------------- periodic ---------------- */
static void core_tick(lv_timer_t *t) {
    update_title_time();
    if (s_cur->tick) s_cur->tick();
    if (clock_timer_poll()) lv_obj_remove_flag(s_alarm, LV_OBJ_FLAG_HIDDEN);
    if (!lv_obj_has_flag(s_alarm, LV_OBJ_FLAG_HIDDEN))
        lv_obj_set_style_bg_opa(s_alarm, (now_ms() / 500) & 1 ? LV_OPA_COVER : LV_OPA_70, 0);
    int ah = settings_auto_home_s();
    if (ah && s_cur != &SCR_HOME && !s_cur->no_auto_home && now_ms() - s_last_input_ms > ah * 1000LL)
        ui_go(&SCR_HOME, LV_SCREEN_LOAD_ANIM_FADE_IN);
}

void ui_init(esp_lcd_panel_handle_t panel) {
    s_panel = panel;
    settings_load();
    lcd_set_rotation(g_cfg.rot);
    gpio_set_direction(PIN_BOOT, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PIN_BOOT, GPIO_PULLUP_ONLY);

    lv_init();
    lv_tick_set_cb(tick_cb);
    s_disp = lv_display_create(SCR_W, SCR_H);
    lv_display_set_color_format(s_disp, LV_COLOR_FORMAT_RGB565);
    size_t sz = SCR_W * 30 * 2;
    void *b1 = heap_caps_malloc(sz, MALLOC_CAP_DMA), *b2 = heap_caps_malloc(sz, MALLOC_CAP_DMA);
    ESP_LOGI(TAG, "draw buffers %p %p", b1, b2);
    lv_display_set_buffers(s_disp, b1, b2, sz, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(s_disp, flush_cb);
    lcd_set_flush_done_cb(flush_done);

    // hold-progress bar + tip on the top layer
    s_hold_bar = ui_box(lv_layer_top(), 0, 0, 0, 4, C_ACCENT, 2);
    lv_obj_add_flag(s_hold_bar, LV_OBJ_FLAG_HIDDEN);
    s_hold_tip = ui_box(lv_layer_top(), 0, 0, 210, 24, 0x0B1222, 12);
    lv_obj_align(s_hold_tip, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_set_style_border_color(s_hold_tip, lv_color_hex(C_ACCENT), 0);
    lv_obj_set_style_border_width(s_hold_tip, 1, 0);
    lv_obj_t *tl = ui_label(s_hold_tip, &font_vi_15, C_TEXT, "Thả: chọn · Giữ tiếp: quay lại");
    lv_obj_center(tl);
    lv_obj_add_flag(s_hold_tip, LV_OBJ_FLAG_HIDDEN);
    alarm_build();

    s_last_input_ms = now_ms();
    ui_go(&SCR_HOME, LV_SCREEN_LOAD_ANIM_NONE);
    lv_timer_create(core_tick, 200, NULL);
    lv_timer_handler();   // first frame before Wi-Fi starts
}

void ui_run(void) {
    int64_t last_log = 0, next_reinit = esp_timer_get_time() + 3000000;
    while (1) {
        int64_t now = esp_timer_get_time();
        if (now > next_reinit) {   // recover the panel if a supply dip reset it
            next_reinit = now + 30000000;
            lcd_reinit(g_cfg.rot);
            lv_obj_invalidate(lv_screen_active());
        }
        if (now - last_log > 30000000) {
            last_log = now;
            ESP_LOGI(TAG, "flushes=%u free heap=%u lv_mem_free=%u", (unsigned)s_flushes,
                     (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT), (unsigned)lv_mon_free());
        }
        uint32_t wait = lv_timer_handler();
        button_poll();
        vTaskDelay(pdMS_TO_TICKS(wait < 10 ? 10 : 10));
    }
}
