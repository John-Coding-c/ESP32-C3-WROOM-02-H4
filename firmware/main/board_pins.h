#pragma once
/* GPIO map taken from the routed PCB (ESP32IoT.kicad_pcb, U3 pad nets). */

/* Shared SPI bus: W25Q32 flash (U6) + microSD slot (J3). */
#define PIN_SPI_SCLK   6   /* module pad 5,  net SCLK    */
#define PIN_SPI_MOSI   7   /* module pad 6,  net MOSI_DI */
#define PIN_SPI_MISO   2   /* module pad 16, net MISO_DO (strapping pin) */
#define PIN_CS_FLASH   10  /* module pad 10, net CS      */
#define PIN_CS_SD      0   /* module pad 18, net CS_SD   */

/* I2C: BME280. The C3 I2C peripheral has no GDMA path. */
#define PIN_I2C_SDA    4
#define PIN_I2C_SCL    3

/* Analog. */
#define PIN_PHOTO      1   /* TEMT6000, ADC1_CH1: supports continuous (DMA) mode */
#define PIN_MIC        5   /* MAX4466,  ADC2_CH0: no ADC DMA on the C3           */
