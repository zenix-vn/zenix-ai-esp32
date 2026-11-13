// Zenix desk clock for Luat Core-ESP32 (ESP32-C3) + 2.4" ST7789V 320x240.
#include "app.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs_flash.h"

app_state_t g_state = {.wifi = WIFI_ST_CONNECTING};
static SemaphoreHandle_t s_mtx;

void state_lock(void) { xSemaphoreTake(s_mtx, portMAX_DELAY); }
void state_unlock(void) { xSemaphoreGive(s_mtx); }

static void net_boot_task(void *arg) {
    net_start();
    vTaskDelete(NULL);
}

void app_main(void) {
    s_mtx = xSemaphoreCreateMutex();
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    esp_lcd_panel_handle_t panel = lcd_start();
    ui_init(panel);   // allocate LVGL draw buffers before Wi-Fi takes the RAM
    xTaskCreate(net_boot_task, "net_boot", 4096, NULL, 5, NULL);
    ui_run();
}
