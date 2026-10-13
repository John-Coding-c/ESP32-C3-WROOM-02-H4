#pragma once
#include <stdint.h>
#include "esp_err.h"

/* Light sensor: ADC1 continuous mode, samples moved by GDMA into a ring buffer. */
esp_err_t light_dma_start(void);
/* Mean of the raw 12-bit samples captured since the previous call (-1 if none). */
int light_dma_mean(void);

/* The microphone is on ADC2, which has no DMA path on the C3: one-shot peak-to-peak. */
esp_err_t mic_init(void);
int mic_peak_to_peak(int samples);
