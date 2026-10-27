#pragma once

#include <stdbool.h>
#include <stdint.h>

// Den bao trang thai. Ho tro LED don mau (PWM, muc cao/thap) hoac LED RGB WS2812.
typedef enum {
    LED_TYPE_NONE = 0,
    LED_TYPE_GPIO_HIGH = 1,  // LED don mau, sang khi chan o muc cao
    LED_TYPE_GPIO_LOW = 2,   // LED don mau, sang khi chan o muc thap
    LED_TYPE_WS2812 = 3,     // LED RGB dia chi hoa (1 bong)
} led_type_t;

typedef enum {
    LED_BOOT,          // dang khoi dong
    LED_WIFI,          // dang ket noi Wi-Fi
    LED_SETUP,         // dang phat Wi-Fi Zin-Setup
    LED_ACTIVATING,    // cho nhap ma kich hoat
    LED_SERVER,        // dang ket noi server
    LED_OFFLINE,       // mat ket noi server, dang thu lai
    LED_IDLE,          // san sang
    LED_LISTENING,     // dang nghe
    LED_THINKING,      // dang cho AI tra loi
    LED_SPEAKING,      // AI dang noi
    LED_ERROR,         // loi (tam thoi)
    LED_TEST,          // nhay thu (tam thoi)
    LED_MODE_COUNT,
} led_mode_t;

void status_led_init(void);
// Doi phan cung LED luc dang chay (tu trang quan ly). gpio < 0 = tat.
void status_led_configure(led_type_t type, int gpio, int brightness);
// Trang thai nen.
void status_led_set(led_mode_t mode);
// Trang thai tam thoi de len trang thai nen trong ms (0 = bo lop tam thoi).
void status_led_overlay(led_mode_t mode, uint32_t ms);
led_mode_t status_led_mode(void);
const char *status_led_mode_name(led_mode_t mode);
bool status_led_gpio_allowed(int gpio);
// Danh sach chan an toan de gan LED (ket thuc bang -1).
extern const int STATUS_LED_SAFE_GPIOS[];
