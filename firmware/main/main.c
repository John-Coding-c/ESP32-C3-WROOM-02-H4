#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "spi_dma.h"
#include "adc_dma.h"

static const char *TAG = "main";

/* Write then read back 1 MiB so the DMA throughput on the SD card is visible. */
static void sd_benchmark(void)
{
    enum { BLOCK = 4096, BLOCKS = 256 };
    static uint8_t blk[BLOCK];
    memset(blk, 0xA5, sizeof(blk));

    int64_t t0 = esp_timer_get_time();
    FILE *f = fopen("/sdcard/dma.bin", "wb");
    if (!f) {
        ESP_LOGE(TAG, "open for write failed");
        return;
    }
    for (int i = 0; i < BLOCKS; i++) {
        fwrite(blk, 1, BLOCK, f);
    }
    fclose(f);
    int64_t t1 = esp_timer_get_time();

    f = fopen("/sdcard/dma.bin", "rb");
    if (!f) {
        ESP_LOGE(TAG, "open for read failed");
        return;
    }
    size_t total = 0, n;
    while ((n = fread(blk, 1, BLOCK, f)) > 0) {
        total += n;
    }
    fclose(f);
    int64_t t2 = esp_timer_get_time();

    ESP_LOGI(TAG, "SD write %.0f KiB/s, read %.0f KiB/s (%u bytes)",
             1024.0 * 1e6 / (t1 - t0), total / 1024.0 * 1e6 / (t2 - t1), (unsigned)total);
}

void app_main(void)
{
    ESP_ERROR_CHECK(spi_dma_init());

    uint32_t id = 0;
    ESP_ERROR_CHECK(flash_read_jedec(&id));
    ESP_LOGI(TAG, "W25Q32 JEDEC id 0x%06lx (expect 0xEF4016)", (unsigned long)id);

    if (sd_dma_mount() == ESP_OK) {
        sd_benchmark();
    } else {
        ESP_LOGW(TAG, "no SD card mounted");
    }

    ESP_ERROR_CHECK(light_dma_start());
    ESP_ERROR_CHECK(mic_init());

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP_LOGI(TAG, "light raw=%d  mic p-p=%d", light_dma_mean(), mic_peak_to_peak(500));
    }
}
