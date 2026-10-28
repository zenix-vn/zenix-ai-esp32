#include "battery.h"

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

static const char *TAG = "battery";

#define SAMPLES 16

static adc_oneshot_unit_handle_t s_adc;
static adc_cali_handle_t s_cali;
static adc_channel_t s_chan;
static battery_info_t s_info = {.percent = -1};

// Duong xa pin Li-ion khi tai nhe (mV -> %), noi suy tuyen tinh giua cac moc.
static const struct {
    int mv;
    int pct;
} CURVE[] = {
    {4150, 100}, {4050, 90}, {3970, 80}, {3900, 70}, {3840, 60}, {3790, 50},
    {3750, 40}, {3710, 30}, {3670, 20}, {3610, 10}, {3500, 5}, {3300, 0},
};

static int mv_to_percent(int mv)
{
    const int n = sizeof(CURVE) / sizeof(CURVE[0]);
    if (mv >= CURVE[0].mv) {
        return 100;
    }
    for (int i = 1; i < n; i++) {
        if (mv >= CURVE[i].mv) {
            return CURVE[i].pct + (mv - CURVE[i].mv) * (CURVE[i - 1].pct - CURVE[i].pct) /
                                      (CURVE[i - 1].mv - CURVE[i].mv);
        }
    }
    return 0;
}

static bool measure(void)
{
    adc_oneshot_chan_cfg_t ch = {.atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_DEFAULT};
    if (adc_oneshot_config_channel(s_adc, s_chan, &ch) != ESP_OK) {
        return false;
    }
    int sum = 0;
    for (int i = 0; i < SAMPLES; i++) {
        int raw = 0;
        if (adc_oneshot_read(s_adc, s_chan, &raw) != ESP_OK) {
            return false;
        }
        sum += raw;
    }
    int raw = sum / SAMPLES;
    int pin_mv = 0;
    if (!s_cali || adc_cali_raw_to_voltage(s_cali, raw, &pin_mv) != ESP_OK) {
        pin_mv = raw * 3300 / 4095;
    }

    s_info.valid = true;
    s_info.pin_mv = pin_mv;
    s_info.battery_mv = pin_mv * CONFIG_XZ_BAT_DIVIDER_X1000 / 1000;
    s_info.present = s_info.battery_mv >= 2800 && s_info.battery_mv <= 4500;
    s_info.percent = s_info.present ? mv_to_percent(s_info.battery_mv) : -1;
    return true;
}

void battery_init(void)
{
    adc_unit_t unit;
    if (adc_oneshot_io_to_channel(CONFIG_XZ_BAT_ADC_GPIO, &unit, &s_chan) != ESP_OK) {
        ESP_LOGE(TAG, "GPIO%d khong co ADC", CONFIG_XZ_BAT_ADC_GPIO);
        return;
    }
    adc_oneshot_unit_init_cfg_t unit_cfg = {.unit_id = unit};
    if (adc_oneshot_new_unit(&unit_cfg, &s_adc) != ESP_OK) {
        ESP_LOGE(TAG, "Khong mo duoc ADC%d", unit + 1);
        return;
    }
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cali = {.unit_id = unit, .chan = s_chan, .atten = ADC_ATTEN_DB_12,
                                            .bitwidth = ADC_BITWIDTH_DEFAULT};
    adc_cali_create_scheme_curve_fitting(&cali, &s_cali);
#endif
    if (measure()) {
        ESP_LOGI(TAG, "GPIO%d = %d mV -> pin %d mV (%s, %d%%)", CONFIG_XZ_BAT_ADC_GPIO, s_info.pin_mv,
                 s_info.battery_mv, s_info.present ? "co pin" : "khong thay pin", s_info.percent);
    }
}

void battery_update(void)
{
    if (!s_adc) {
        return;
    }
    measure();
}

void battery_get(battery_info_t *out)
{
    *out = s_info;
}
