#pragma once

#include "cJSON.h"

// Trang quan ly thiet bi (Wi-Fi, server, am luong, khoi dong lai...).
// Chay o ca che do cai dat (AP Zin-Setup) lan khi da vao mang nha.
void web_server_start(void);

// Do main.c cung cap: them trang thai ung dung (ket noi server, ma kich hoat...) vao JSON.
void app_fill_status(cJSON *root);
