#include "adc_dma.h"

#include "esp_adc/adc_continuous.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_check.h"
#include "board_pins.h"

#define TAG          "adc_dma"
#define LIGHT_CH     ADC_CHANNEL_1      /* GPIO1 = ADC1_CH1 */
#define MIC_CH       ADC_CHANNEL_0      /* GPIO5 = ADC2_CH0 */
#define FRAME_BYTES  256
#define READ_BYTES   1024

static adc_continuous_handle_t s_adc;
static adc_oneshot_unit_handle_t s_mic;

esp_err_t light_dma_start(void)
{
    adc_continuous_handle_cfg_t h = {
        .max_store_buf_size = 4096,
        .conv_frame_size = FRAME_BYTES,
    };
    ESP_RETURN_ON_ERROR(adc_continuous_new_handle(&h, &s_adc), TAG, "handle");

    adc_digi_pattern_config_t pat = {
        .atten = ADC_ATTEN_DB_12,
        .channel = LIGHT_CH,
        .unit = ADC_UNIT_1,
        .bit_width = ADC_BITWIDTH_12,
    };
    adc_continuous_config_t c = {
        .pattern_num = 1,
        .adc_pattern = &pat,
        .sample_freq_hz = 10 * 1000,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE2,
    };
    ESP_RETURN_ON_ERROR(adc_continuous_config(s_adc, &c), TAG, "config");
    return adc_continuous_start(s_adc);
}

int light_dma_mean(void)
{
    static uint8_t buf[READ_BYTES];
    uint32_t got = 0;
    int64_t sum = 0;
    int count = 0;

    /* Drain everything the DMA has queued since the last call. */
    while (adc_continuous_read(s_adc, buf, sizeof(buf), &got, 0) == ESP_OK) {
        for (uint32_t i = 0; i + SOC_ADC_DIGI_RESULT_BYTES <= got; i += SOC_ADC_DIGI_RESULT_BYTES) {
            adc_digi_output_data_t *p = (adc_digi_output_data_t *)&buf[i];
            sum += p->type2.data;
            count++;
        }
    }
    return count ? (int)(sum / count) : -1;
}

esp_err_t mic_init(void)
{
    adc_oneshot_unit_init_cfg_t u = { .unit_id = ADC_UNIT_2 };
    ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&u, &s_mic), TAG, "unit");
    adc_oneshot_chan_cfg_t c = { .atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_12 };
    return adc_oneshot_config_channel(s_mic, MIC_CH, &c);
}

int mic_peak_to_peak(int samples)
{
    int lo = 4095, hi = 0, v;
    for (int i = 0; i < samples; i++) {
        if (adc_oneshot_read(s_mic, MIC_CH, &v) != ESP_OK) {
            return -1;
        }
        if (v < lo) lo = v;
        if (v > hi) hi = v;
    }
    return hi - lo;
}
