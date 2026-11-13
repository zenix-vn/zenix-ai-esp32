// ST7789V 2.4" 240x320 (TN) on SPI.  SCK=GPIO2, MOSI=GPIO3.
// CS/DC/RST are some order of GPIO7/5/4 (seller did not document it). On first boot the
// firmware shows 1..6 for each candidate order; press BOOT while a number is visible and the
// choice is stored in NVS.
#include <string.h>
#include "app.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_io_spi.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_st7789.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"

#define PIN_SCK  2
#define PIN_MOSI_DEFAULT 3

// Per-board MOSI override. The old board (MAC ..4f:a4) had GPIO3 damaged; its display MOSI pad is
// now wired to GPIO10 (header pin 21) instead.
static int pick_mosi(void) {
    static const struct { uint8_t mac[6]; int mosi; } BOARDS[] = {
        {{0x10, 0x20, 0xba, 0xce, 0x4f, 0xa4}, 10},
    };
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    for (size_t i = 0; i < sizeof BOARDS / sizeof BOARDS[0]; i++)
        if (!memcmp(mac, BOARDS[i].mac, 6)) return BOARDS[i].mosi;
    return PIN_MOSI_DEFAULT;
}
#define PIN_BOOT 9
#define NATIVE_W 240
#define NATIVE_H 320

static const char *TAG = "lcd";
static const int PERM[6][3] = {{7, 5, 4}, {7, 4, 5}, {5, 4, 7}, {4, 5, 7}, {5, 7, 4}, {4, 7, 5}};

static esp_lcd_panel_io_handle_t s_io;
static esp_lcd_panel_handle_t s_panel;
static SemaphoreHandle_t s_done;
static void (*s_flush_cb)(void);

static bool on_trans_done(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *e, void *ctx) {
    if (s_flush_cb) { s_flush_cb(); return false; }
    BaseType_t hp = pdFALSE;
    xSemaphoreGiveFromISR(s_done, &hp);
    return hp == pdTRUE;
}

void lcd_set_flush_done_cb(void (*cb)(void)) { s_flush_cb = cb; }

static void panel_open(int cs, int dc, int rst) {
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num = cs, .dc_gpio_num = dc, .spi_mode = 0, .pclk_hz = 40 * 1000 * 1000,
        .trans_queue_depth = 4, .on_color_trans_done = on_trans_done,
        .lcd_cmd_bits = 8, .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_cfg, &s_io));
    esp_lcd_panel_dev_config_t pcfg = {
        .reset_gpio_num = rst, .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB, .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(s_io, &pcfg, &s_panel));
    esp_lcd_panel_reset(s_panel);
    esp_lcd_panel_init(s_panel);
    esp_lcd_panel_invert_color(s_panel, true);    // this panel shows inverted colours without it
    esp_lcd_panel_disp_on_off(s_panel, true);
}

static void panel_close(void) {
    esp_lcd_panel_del(s_panel);
    esp_lcd_panel_io_del(s_io);
    s_panel = NULL; s_io = NULL;
}

// --- tiny drawing helpers used only by the pin-selection screen ---
static void blit(int x, int y, int w, int h, uint16_t *px) {
    esp_lcd_panel_draw_bitmap(s_panel, x, y, x + w, y + h, px);
    xSemaphoreTake(s_done, pdMS_TO_TICKS(500));
}

static void draw_digit_screen(int n) {
    // 5x7 glyphs for 1..6, column-major, LSB = top row
    static const uint8_t g[6][5] = {{0x00,0x42,0x7F,0x40,0x00},{0x42,0x61,0x51,0x49,0x46},
        {0x21,0x41,0x45,0x4B,0x31},{0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},
        {0x3C,0x4A,0x49,0x49,0x30}};
    const uint16_t bg = 0x1F00, fg = 0xFFFF;   // blue / white, already byte-swapped for SPI
    const int S = 20, rows = 16;
    uint16_t *buf = heap_caps_malloc(NATIVE_W * rows * 2, MALLOC_CAP_DMA);
    for (int y0 = 0; y0 < NATIVE_H; y0 += rows) {
        for (int y = 0; y < rows; y++)
            for (int x = 0; x < NATIVE_W; x++) {
                int gx = (x - (NATIVE_W - 5 * S) / 2) / S, gy = (y0 + y - (NATIVE_H - 7 * S) / 2) / S;
                bool on = x >= (NATIVE_W - 5 * S) / 2 && gx < 5 && y0 + y >= (NATIVE_H - 7 * S) / 2 && gy < 7 &&
                          (g[n - 1][gx] >> gy & 1);
                buf[y * NATIVE_W + x] = on ? fg : bg;
            }
        blit(0, y0, NATIVE_W, rows, buf);
    }
    free(buf);
}

static int select_pins(void) {
    gpio_set_direction(PIN_BOOT, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PIN_BOOT, GPIO_PULLUP_ONLY);
    while (1) {
        for (int p = 0; p < 6; p++) {
            ESP_LOGI(TAG, "try #%d CS=%d DC=%d RST=%d", p + 1, PERM[p][0], PERM[p][1], PERM[p][2]);
            panel_open(PERM[p][0], PERM[p][1], PERM[p][2]);
            draw_digit_screen(p + 1);
            for (int k = 0; k < 75; k++) {
                vTaskDelay(pdMS_TO_TICKS(40));
                if (gpio_get_level(PIN_BOOT) == 0) {
                    while (gpio_get_level(PIN_BOOT) == 0) vTaskDelay(pdMS_TO_TICKS(20));
                    ESP_LOGI(TAG, "locked #%d", p + 1);
                    return p;
                }
            }
            panel_close();
            for (int i = 0; i < 3; i++) gpio_reset_pin(PERM[p][i]);
        }
    }
}

void lcd_set_rotation(int rot) {
    esp_lcd_panel_swap_xy(s_panel, true);
    if (rot) esp_lcd_panel_mirror(s_panel, false, true);
    else esp_lcd_panel_mirror(s_panel, true, false);
}

// Re-send the init sequence. A supply dip (Wi-Fi TX bursts on weak USB power) can reset the
// ST7789 back to sleep mode while the ESP32 keeps running; this brings it back.
void lcd_reinit(int rot) {
    esp_lcd_panel_init(s_panel);
    esp_lcd_panel_invert_color(s_panel, true);
    lcd_set_rotation(rot);
    esp_lcd_panel_disp_on_off(s_panel, true);
}

esp_lcd_panel_handle_t lcd_start(void) {
    s_done = xSemaphoreCreateBinary();
    int mosi = pick_mosi();
    ESP_LOGI(TAG, "MOSI=GPIO%d", mosi);
    spi_bus_config_t bus = {.mosi_io_num = mosi, .miso_io_num = -1, .sclk_io_num = PIN_SCK,
                            .quadwp_io_num = -1, .quadhd_io_num = -1,
                            .max_transfer_sz = SCR_W * 40 * 2};
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));

    nvs_handle_t nvs;
    int8_t perm = -1;
    if (nvs_open("lcd", NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_get_i8(nvs, "perm", &perm);
        if (perm < 0 || perm > 5) perm = 1;   // verified on hardware: CS=7 DC=4 RST=5
        if (perm < 0) {
            perm = select_pins();   // leaves the panel open
            nvs_set_i8(nvs, "perm", perm);
            nvs_commit(nvs);
        } else {
            panel_open(PERM[perm][0], PERM[perm][1], PERM[perm][2]);
        }
        nvs_close(nvs);
    }
    ESP_LOGI(TAG, "LCD pins CS=%d DC=%d RST=%d", PERM[perm][0], PERM[perm][1], PERM[perm][2]);
    return s_panel;
}
