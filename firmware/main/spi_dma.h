#pragma once
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

/* Bring up SPI2 with a GDMA channel and attach the flash device. */
esp_err_t spi_dma_init(void);

/* Mount the microSD card (SPI mode, DMA transfers) at /sdcard. */
esp_err_t sd_dma_mount(void);

/* W25Q32JV. All transfers go through DMA-capable bounce buffers. */
esp_err_t flash_read_jedec(uint32_t *id);
esp_err_t flash_read(uint32_t addr, void *dst, size_t len);
esp_err_t flash_erase_sector(uint32_t addr);                        /* 4 KiB */
esp_err_t flash_write(uint32_t addr, const void *src, size_t len);  /* sector must be erased */
