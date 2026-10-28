#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "driver/i2c_master.h"

// Mic va loa cung chay 16 kHz mono; moi khung Opus 60 ms = 960 mau.
#define AUDIO_SAMPLE_RATE   16000
#define AUDIO_FRAME_MS      60
#define AUDIO_FRAME_SAMPLES (AUDIO_SAMPLE_RATE / 1000 * AUDIO_FRAME_MS)

esp_err_t audio_init(i2c_master_bus_handle_t bus);

// Doc 1 khung tu mic va ma hoa Opus. Tra ve so byte (<=0 neu loi).
int audio_record_opus(uint8_t *out, size_t out_cap);
// Giai ma 1 goi Opus tu server va phat ra loa.
void audio_play_opus(const uint8_t *data, size_t len);
void audio_reset_decoder(void);
// Phat 3 not ngan (do-mi-sol) de thu loa, khong can server.
void audio_play_test_tone(void);

void audio_set_volume(int volume);
int audio_get_volume(void);
