#pragma once

#include "esp_err.h"

typedef struct {
    char device_id[18];  // MAC Wi-Fi
    char client_id[37];  // UUID luu trong NVS
    char ws_url[256];
    char ws_token[256];
} xz_server_cfg_t;

// Goi OTA server: lay websocket url/token; neu thiet bi chua kich hoat thi in ma 6 so
// va cho nguoi dung nhap ma tren xiaozhi.me. Chi tra ve khi da co cau hinh websocket.
esp_err_t xz_ota_activate(xz_server_cfg_t *cfg);

// Ma kich hoat dang cho nguoi dung nhap ("" neu khong co) — hien tren trang quan ly.
const char *xz_ota_activation_code(void);
const char *xz_ota_activation_message(void);
