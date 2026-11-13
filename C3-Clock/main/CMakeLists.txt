file(GLOB FONT_SRCS "fonts/*.c")
idf_component_register(
    SRCS "main.c" "lcd.c" "net.c" "settings.c" "lunar.c" "ui_core.c" "scr_home.c" "scr_apps.c"
         "app_weather.c" "app_calendar.c" "app_clock.c" "app_settings.c" ${FONT_SRCS}
    INCLUDE_DIRS "."
    REQUIRES lvgl esp_lcd esp_driver_spi esp_driver_gpio nvs_flash esp_wifi esp_netif
             esp_event esp_http_client esp_http_server lwip esp_timer)
