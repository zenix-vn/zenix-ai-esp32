#include "status_led.h"

#include <math.h>
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "sdkconfig.h"

static const char *TAG = "led";

// Chi cac chan da ra header JP1 va firmware chua dung (xem docs/BOARD.md).
// Bo: codec/I2C (7-13, 53), Wi-Fi SDIO (14-19, 54), USB (24, 25), RS485 (26, 27), Ethernet (28-31, 49-52),
// BOOT (35), UART0 (37, 38), the nho + nguon the (39-45), ES7210 (48), GPIO36/34 (strapping/RMII).
const int STATUS_LED_SAFE_GPIOS[] = {1, 2, 3, 4, 5, 20, 32, 33, 46, 47, -1};

typedef enum { FX_SOLID, FX_BLINK, FX_BREATHE, FX_DOUBLE } fx_t;

typedef struct {
    const char *name;
    uint8_t r, g, b;
    fx_t fx;
    uint16_t period_ms;
    float level;  // do sang toi da cua hieu ung (0..1)
} led_pattern_t;

static const led_pattern_t PATTERNS[LED_MODE_COUNT] = {
    [LED_BOOT]       = {"boot",       255, 255, 255, FX_BREATHE, 1600, 0.6f},
    [LED_WIFI]       = {"wifi",       0,   180, 255, FX_BLINK,   400,  1.0f},
    [LED_SETUP]      = {"setup",      0,   60,  255, FX_BLINK,   1200, 1.0f},
    [LED_ACTIVATING] = {"activating", 170, 0,   255, FX_DOUBLE,  1600, 1.0f},
    [LED_SERVER]     = {"server",     255, 150, 0,   FX_BLINK,   400,  1.0f},
    [LED_OFFLINE]    = {"offline",    255, 60,  0,   FX_DOUBLE,  2500, 1.0f},
    [LED_IDLE]       = {"idle",       0,   255, 70,  FX_SOLID,   0,    0.15f},
    [LED_LISTENING]  = {"listening",  0,   220, 255, FX_SOLID,   0,    1.0f},
    [LED_THINKING]   = {"thinking",   170, 0,   255, FX_BREATHE, 900,  1.0f},
    [LED_SPEAKING]   = {"speaking",   60,  120, 255, FX_BREATHE, 600,  1.0f},
    [LED_ERROR]      = {"error",      255, 0,   0,   FX_BLINK,   250,  1.0f},
    [LED_TEST]       = {"test",       255, 255, 255, FX_BLINK,   200,  1.0f},
};

static SemaphoreHandle_t s_lock;
static led_type_t s_type = LED_TYPE_NONE;
static int s_gpio = -1;
static float s_brightness = 0.6f;
static led_strip_handle_t s_strip;
static bool s_ledc_on;

static volatile led_mode_t s_base = LED_BOOT;
static volatile led_mode_t s_overlay = LED_MODE_COUNT;  // LED_MODE_COUNT = khong co
static volatile int64_t s_overlay_until;

bool status_led_gpio_allowed(int gpio)
{
    if (gpio == CONFIG_XZ_TOUCH_GPIO) {
        return false;  // chan nay la ngo vao cua cam bien cham
    }
    for (const int *p = STATUS_LED_SAFE_GPIOS; *p >= 0; p++) {
        if (*p == gpio) {
            return true;
        }
    }
    return false;
}

static float intensity(const led_pattern_t *p, int64_t t_ms)
{
    if (p->fx == FX_SOLID || p->period_ms == 0) {
        return p->level;
    }
    float phase = (float)(t_ms % p->period_ms) / p->period_ms;
    switch (p->fx) {
    case FX_BLINK:
        return phase < 0.5f ? p->level : 0;
    case FX_BREATHE: {
        float v = (1 - cosf(phase * 2 * (float)M_PI)) / 2;
        return p->level * (0.08f + 0.92f * v * v);  // binh phuong: mat hon voi mat nguoi
    }
    case FX_DOUBLE:  // hai lan nhay ngan dau chu ky
        return (phase < 0.08f || (phase > 0.16f && phase < 0.24f)) ? p->level : 0;
    default:
        return 0;
    }
}

static void hw_release(void)
{
    if (s_strip) {
        led_strip_clear(s_strip);
        led_strip_del(s_strip);
        s_strip = NULL;
    }
    if (s_ledc_on) {
        ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, s_type == LED_TYPE_GPIO_LOW ? 1 : 0);
        s_ledc_on = false;
    }
    if (s_gpio >= 0) {
        gpio_reset_pin(s_gpio);
    }
}

static void hw_setup(void)
{
    if (s_gpio < 0 || s_type == LED_TYPE_NONE) {
        return;
    }
    if (s_type == LED_TYPE_WS2812) {
        led_strip_config_t strip = {
            .strip_gpio_num = s_gpio,
            .max_leds = 1,
            .led_model = LED_MODEL_WS2812,
            .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        };
        led_strip_rmt_config_t rmt = {.clk_src = RMT_CLK_SRC_DEFAULT, .resolution_hz = 10 * 1000 * 1000};
        if (led_strip_new_rmt_device(&strip, &rmt, &s_strip) != ESP_OK) {
            ESP_LOGE(TAG, "WS2812 init failed on GPIO%d", s_gpio);
            s_strip = NULL;
        }
        return;
    }
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 4000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_channel_config_t ch = {
        .gpio_num = s_gpio,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .flags.output_invert = s_type == LED_TYPE_GPIO_LOW,
    };
    s_ledc_on = ledc_timer_config(&timer) == ESP_OK && ledc_channel_config(&ch) == ESP_OK;
    if (!s_ledc_on) {
        ESP_LOGE(TAG, "LEDC init failed on GPIO%d", s_gpio);
    }
}

static void render(float v, const led_pattern_t *p)
{
    v *= s_brightness;
    if (s_strip) {
        led_strip_set_pixel(s_strip, 0, (uint32_t)(p->r * v), (uint32_t)(p->g * v), (uint32_t)(p->b * v));
        led_strip_refresh(s_strip);
    } else if (s_ledc_on) {
        // Hieu chinh gamma cho LED don mau de buoc sang/toi muot.
        uint32_t duty = (uint32_t)(powf(v, 2.2f) * 1023);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    }
}

led_mode_t status_led_mode(void)
{
    if (s_overlay < LED_MODE_COUNT && esp_timer_get_time() / 1000 < s_overlay_until) {
        return s_overlay;
    }
    return s_base;
}

static void led_task(void *arg)
{
    while (true) {
        xSemaphoreTake(s_lock, portMAX_DELAY);
        const led_pattern_t *p = &PATTERNS[status_led_mode()];
        render(intensity(p, esp_timer_get_time() / 1000), p);
        xSemaphoreGive(s_lock);
        vTaskDelay(pdMS_TO_TICKS(25));
    }
}

void status_led_configure(led_type_t type, int gpio, int brightness)
{
    if (type != LED_TYPE_NONE && !status_led_gpio_allowed(gpio)) {
        ESP_LOGW(TAG, "GPIO%d khong nam trong danh sach an toan, tat LED", gpio);
        type = LED_TYPE_NONE;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    hw_release();
    s_type = type;
    s_gpio = type == LED_TYPE_NONE ? -1 : gpio;
    s_brightness = (brightness < 5 ? 5 : brightness > 100 ? 100 : brightness) / 100.0f;
    hw_setup();
    xSemaphoreGive(s_lock);
    ESP_LOGI(TAG, "LED: type %d, GPIO%d, brightness %d%%", type, s_gpio, brightness);
}

void status_led_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    xTaskCreate(led_task, "led", 3072, NULL, 3, NULL);
}

void status_led_set(led_mode_t mode)
{
    s_base = mode;
}

void status_led_overlay(led_mode_t mode, uint32_t ms)
{
    s_overlay_until = esp_timer_get_time() / 1000 + ms;
    s_overlay = ms ? mode : LED_MODE_COUNT;
}

const char *status_led_mode_name(led_mode_t mode)
{
    return mode < LED_MODE_COUNT ? PATTERNS[mode].name : "?";
}
