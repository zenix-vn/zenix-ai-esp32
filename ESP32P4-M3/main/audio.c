#include "audio.h"

#include "driver/i2s_std.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "esp_log.h"
#include "esp_opus_dec.h"
#include "esp_opus_enc.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "audio";

// Server co the gui khung toi 120 ms; du phong bo dem giai ma theo do.
#define DECODE_MAX_SAMPLES (AUDIO_SAMPLE_RATE / 1000 * 120)

static esp_codec_dev_handle_t s_dev;
static void *s_enc;
static void *s_dec;
static int s_enc_in_bytes;
static int s_volume = CONFIG_XZ_VOLUME;
static int16_t *s_pcm_in;
static int16_t *s_pcm_out;

static void create_i2s(i2s_chan_handle_t *tx, i2s_chan_handle_t *rx)
{
    i2s_chan_config_t chan = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan.dma_desc_num = 6;
    chan.dma_frame_num = 240;
    chan.auto_clear_after_cb = true;
    ESP_ERROR_CHECK(i2s_new_channel(&chan, tx, rx));

    i2s_std_config_t std = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = CONFIG_XZ_I2S_MCLK,
            .bclk = CONFIG_XZ_I2S_BCLK,
            .ws = CONFIG_XZ_I2S_WS,
            .dout = CONFIG_XZ_I2S_DOUT,
            .din = CONFIG_XZ_I2S_DIN,
        },
    };
    std.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(*tx, &std));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(*rx, &std));
    ESP_ERROR_CHECK(i2s_channel_enable(*tx));
    ESP_ERROR_CHECK(i2s_channel_enable(*rx));
}

static esp_err_t open_codec(i2c_master_bus_handle_t bus, i2s_chan_handle_t tx, i2s_chan_handle_t rx)
{
    audio_codec_i2s_cfg_t i2s_cfg = {.port = I2S_NUM_0, .rx_handle = rx, .tx_handle = tx};
    const audio_codec_data_if_t *data_if = audio_codec_new_i2s_data(&i2s_cfg);

    // esp_codec_dev dung dia chi 8-bit (0x18 << 1 = 0x30).
    audio_codec_i2c_cfg_t i2c_cfg = {.port = I2C_NUM_0, .addr = CONFIG_XZ_ES8311_ADDR << 1, .bus_handle = bus};
    const audio_codec_ctrl_if_t *ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();
    if (!data_if || !ctrl_if || !gpio_if) {
        return ESP_FAIL;
    }

    es8311_codec_cfg_t es_cfg = {
        .ctrl_if = ctrl_if,
        .gpio_if = gpio_if,
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH,
        .pa_pin = CONFIG_XZ_PA_GPIO,
        .use_mclk = true,
        .hw_gain = {.pa_voltage = 5.0, .codec_dac_voltage = 3.3},
    };
    const audio_codec_if_t *codec_if = es8311_codec_new(&es_cfg);
    if (!codec_if) {
        return ESP_FAIL;
    }

    esp_codec_dev_cfg_t dev_cfg = {.dev_type = ESP_CODEC_DEV_TYPE_IN_OUT, .codec_if = codec_if, .data_if = data_if};
    s_dev = esp_codec_dev_new(&dev_cfg);
    esp_codec_dev_sample_info_t fs = {.bits_per_sample = 16, .channel = 1, .sample_rate = AUDIO_SAMPLE_RATE};
    if (!s_dev || esp_codec_dev_open(s_dev, &fs) != ESP_CODEC_DEV_OK) {
        return ESP_FAIL;
    }
    esp_codec_dev_set_in_gain(s_dev, CONFIG_XZ_MIC_GAIN);
    esp_codec_dev_set_out_vol(s_dev, s_volume);
    return ESP_OK;
}

static esp_err_t open_opus(void)
{
    esp_opus_enc_config_t enc_cfg = {
        .sample_rate = AUDIO_SAMPLE_RATE,
        .channel = ESP_AUDIO_MONO,
        .bits_per_sample = ESP_AUDIO_BIT16,
        .bitrate = ESP_OPUS_BITRATE_AUTO,
        .frame_duration = ESP_OPUS_ENC_FRAME_DURATION_60_MS,
        .application_mode = ESP_OPUS_ENC_APPLICATION_AUDIO,
        .complexity = 0,
        .enable_fec = false,
        .enable_dtx = true,
        .enable_vbr = true,
    };
    if (esp_opus_enc_open(&enc_cfg, sizeof(enc_cfg), &s_enc) != ESP_AUDIO_ERR_OK) {
        return ESP_FAIL;
    }
    int out_bytes = 0;
    esp_opus_enc_get_frame_size(s_enc, &s_enc_in_bytes, &out_bytes);

    // Server thuong gui 24 kHz; Opus cho phep giai ma thang ra 16 kHz nen khong can resample.
    esp_opus_dec_cfg_t dec_cfg = {
        .sample_rate = AUDIO_SAMPLE_RATE,
        .channel = ESP_AUDIO_MONO,
        .frame_duration = ESP_OPUS_DEC_FRAME_DURATION_INVALID,
        .self_delimited = false,
    };
    if (esp_opus_dec_open(&dec_cfg, sizeof(dec_cfg), &s_dec) != ESP_AUDIO_ERR_OK) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t audio_init(i2c_master_bus_handle_t bus)
{
    s_pcm_in = heap_caps_malloc(AUDIO_FRAME_SAMPLES * sizeof(int16_t), MALLOC_CAP_DEFAULT);
    s_pcm_out = heap_caps_malloc(DECODE_MAX_SAMPLES * sizeof(int16_t), MALLOC_CAP_DEFAULT);

    i2s_chan_handle_t tx, rx;
    create_i2s(&tx, &rx);
    if (open_codec(bus, tx, rx) != ESP_OK) {
        ESP_LOGE(TAG, "ES8311 init failed (addr 0x%02x) — kiem tra chan I2C/I2S trong menuconfig", CONFIG_XZ_ES8311_ADDR);
        return ESP_FAIL;
    }
    if (open_opus() != ESP_OK) {
        ESP_LOGE(TAG, "Opus init failed");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Audio ready: %d Hz, frame %d ms, opus in %d bytes", AUDIO_SAMPLE_RATE, AUDIO_FRAME_MS, s_enc_in_bytes);
    return ESP_OK;
}

int audio_record_opus(uint8_t *out, size_t out_cap)
{
    if (esp_codec_dev_read(s_dev, s_pcm_in, s_enc_in_bytes) != ESP_CODEC_DEV_OK) {
        return -1;
    }
    esp_audio_enc_in_frame_t in = {.buffer = (uint8_t *)s_pcm_in, .len = s_enc_in_bytes};
    esp_audio_enc_out_frame_t enc_out = {.buffer = out, .len = out_cap};
    if (esp_opus_enc_process(s_enc, &in, &enc_out) != ESP_AUDIO_ERR_OK) {
        return -1;
    }
    return enc_out.encoded_bytes;
}

void audio_play_opus(const uint8_t *data, size_t len)
{
    esp_audio_dec_in_raw_t raw = {.buffer = (uint8_t *)data, .len = len};
    esp_audio_dec_out_frame_t frame = {.buffer = (uint8_t *)s_pcm_out, .len = DECODE_MAX_SAMPLES * sizeof(int16_t)};
    esp_audio_dec_info_t info = {0};
    esp_audio_err_t ret = esp_opus_dec_decode(s_dec, &raw, &frame, &info);
    if (ret != ESP_AUDIO_ERR_OK) {
        ESP_LOGW(TAG, "Opus decode failed: %d", ret);
        return;
    }
    if (frame.decoded_size > 0) {
        esp_codec_dev_write(s_dev, s_pcm_out, frame.decoded_size);
    }
}

void audio_reset_decoder(void)
{
    esp_opus_dec_reset(s_dec);
}

void audio_set_volume(int volume)
{
    s_volume = volume < 0 ? 0 : (volume > 100 ? 100 : volume);
    esp_codec_dev_set_out_vol(s_dev, s_volume);
}

int audio_get_volume(void)
{
    return s_volume;
}
