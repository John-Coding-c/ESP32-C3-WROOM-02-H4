idf_component_register(
    SRCS "main.c" "spi_dma.c" "adc_dma.c"
    INCLUDE_DIRS "."
    REQUIRES esp_adc driver fatfs sdmmc esp_timer)
