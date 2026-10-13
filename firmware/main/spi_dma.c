#include "spi_dma.h"

#include <string.h>
#include "driver/spi_master.h"
#include "driver/sdspi_host.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "board_pins.h"

#define TAG     "spi_dma"
#define HOST    SPI2_HOST
#define CHUNK   256               /* one flash page */
#define BUF_SZ  (4 + CHUNK)

static spi_device_handle_t s_flash;
static uint8_t *s_tx, *s_rx;      /* DMA-capable internal RAM */

esp_err_t spi_dma_init(void)
{
    spi_bus_config_t bus = {
        .mosi_io_num = PIN_SPI_MOSI,
        .miso_io_num = PIN_SPI_MISO,
        .sclk_io_num = PIN_SPI_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(HOST, &bus, SPI_DMA_CH_AUTO), TAG, "bus init");

    spi_device_interface_config_t dev = {
        .clock_speed_hz = 20 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = PIN_CS_FLASH,
        .queue_size = 2,
    };
    ESP_RETURN_ON_ERROR(spi_bus_add_device(HOST, &dev, &s_flash), TAG, "add flash");

    s_tx = heap_caps_malloc(BUF_SZ, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    s_rx = heap_caps_malloc(BUF_SZ, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    return (s_tx && s_rx) ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t sd_dma_mount(void)
{
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = HOST;                      /* reuse the bus initialised above */
    host.max_freq_khz = 20000;

    sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot.gpio_cs = PIN_CS_SD;
    slot.host_id = HOST;

    esp_vfs_fat_sdmmc_mount_config_t cfg = {
        .format_if_mount_failed = false,
        .max_files = 4,
        .allocation_unit_size = 16 * 1024,
    };
    sdmmc_card_t *card;
    esp_err_t err = esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot, &cfg, &card);
    if (err == ESP_OK) {
        sdmmc_card_print_info(stdout, card);
    }
    return err;
}

/* Full-duplex transfer of n bytes from s_tx into s_rx as one DMA transaction. */
static esp_err_t xfer(size_t n)
{
    spi_transaction_t t = {
        .length = n * 8,
        .tx_buffer = s_tx,
        .rx_buffer = s_rx,
    };
    return spi_device_transmit(s_flash, &t);
}

static esp_err_t write_enable(void)
{
    s_tx[0] = 0x06;
    return xfer(1);
}

static esp_err_t wait_idle(uint32_t timeout_ms)
{
    TickType_t end = xTaskGetTickCount() + pdMS_TO_TICKS(timeout_ms);
    do {
        s_tx[0] = 0x05;
        s_tx[1] = 0;
        ESP_RETURN_ON_ERROR(xfer(2), TAG, "rdsr");
        if (!(s_rx[1] & 1)) {
            return ESP_OK;
        }
        vTaskDelay(1);
    } while (xTaskGetTickCount() < end);
    return ESP_ERR_TIMEOUT;
}

esp_err_t flash_read_jedec(uint32_t *id)
{
    memset(s_tx, 0, 4);
    s_tx[0] = 0x9F;
    ESP_RETURN_ON_ERROR(xfer(4), TAG, "jedec");
    *id = ((uint32_t)s_rx[1] << 16) | (s_rx[2] << 8) | s_rx[3];
    return ESP_OK;
}

esp_err_t flash_read(uint32_t addr, void *dst, size_t len)
{
    uint8_t *out = dst;
    while (len) {
        size_t n = len > CHUNK ? CHUNK : len;
        s_tx[0] = 0x03;
        s_tx[1] = addr >> 16;
        s_tx[2] = addr >> 8;
        s_tx[3] = addr;
        memset(s_tx + 4, 0, n);
        ESP_RETURN_ON_ERROR(xfer(4 + n), TAG, "read");
        memcpy(out, s_rx + 4, n);
        out += n;
        addr += n;
        len -= n;
    }
    return ESP_OK;
}

esp_err_t flash_erase_sector(uint32_t addr)
{
    ESP_RETURN_ON_ERROR(write_enable(), TAG, "wren");
    s_tx[0] = 0x20;
    s_tx[1] = addr >> 16;
    s_tx[2] = addr >> 8;
    s_tx[3] = addr;
    ESP_RETURN_ON_ERROR(xfer(4), TAG, "erase");
    return wait_idle(500);
}

esp_err_t flash_write(uint32_t addr, const void *src, size_t len)
{
    const uint8_t *in = src;
    while (len) {
        size_t room = 256 - (addr & 0xFF);   /* never cross a page boundary */
        size_t n = len > room ? room : len;
        ESP_RETURN_ON_ERROR(write_enable(), TAG, "wren");
        s_tx[0] = 0x02;
        s_tx[1] = addr >> 16;
        s_tx[2] = addr >> 8;
        s_tx[3] = addr;
        memcpy(s_tx + 4, in, n);
        ESP_RETURN_ON_ERROR(xfer(4 + n), TAG, "program");
        ESP_RETURN_ON_ERROR(wait_idle(10), TAG, "busy");
        in += n;
        addr += n;
        len -= n;
    }
    return ESP_OK;
}
