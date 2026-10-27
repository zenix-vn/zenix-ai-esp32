idf_component_register(
    SRCS "main.c" "board.c" "audio.c" "net_wifi.c" "ota_activate.c" "xz_protocol.c" "mcp.c"
         "settings.c" "web_server.c" "captive_dns.c" "status_led.c"
    INCLUDE_DIRS "."
    EMBED_TXTFILES "web/index.html"
    PRIV_REQUIRES nvs_flash esp_wifi esp_netif esp_event esp_http_client esp_http_server esp_driver_gpio
                  esp_driver_i2c esp_driver_i2s esp_driver_ledc esp_timer esp_app_format mbedtls
)
