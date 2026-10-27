#include "board.h"

#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "board";

static i2c_master_bus_handle_t s_bus;

i2c_master_bus_handle_t board_i2c_init(void)
{
    i2c_master_bus_config_t cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = CONFIG_XZ_I2C_SDA,
        .scl_io_num = CONFIG_XZ_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&cfg, &s_bus));

    int found = 0;
    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        if (i2c_master_probe(s_bus, addr, 50) == ESP_OK) {
            ESP_LOGI(TAG, "I2C device at 0x%02x", addr);
            found++;
        }
    }
    ESP_LOGI(TAG, "I2C scan (SDA=%d SCL=%d): %d device(s)", CONFIG_XZ_I2C_SDA, CONFIG_XZ_I2C_SCL, found);
    return s_bus;
}

bool board_i2c_has_device(uint8_t addr)
{
    return s_bus && i2c_master_probe(s_bus, addr, 50) == ESP_OK;
}

void board_button_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << CONFIG_XZ_BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io));
}

bool board_button_pressed(void)
{
    return gpio_get_level(CONFIG_XZ_BUTTON_GPIO) == 0;
}
