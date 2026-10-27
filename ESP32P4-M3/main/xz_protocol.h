#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "cJSON.h"
#include "ota_activate.h"

// Lop giao thuc WebSocket theo chuan xiaozhi (Protocol-Version 1: khung Opus tho).
typedef struct {
    void (*on_json)(const cJSON *root);                // Chay trong task websocket
    void (*on_audio)(const uint8_t *data, size_t len); // Khung Opus TTS tu server
    void (*on_closed)(void);
} xz_callbacks_t;

void xz_protocol_init(const xz_server_cfg_t *cfg, const xz_callbacks_t *cb);
// Mo ket noi, gui "hello" va cho server tra "hello" (toi da 10 s).
bool xz_open(void);
void xz_close(void);
bool xz_is_open(void);

// Gan session_id vao msg roi gui; khong giai phong msg.
bool xz_send_json(cJSON *msg);
bool xz_send_audio(const uint8_t *data, size_t len);
