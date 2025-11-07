#ifndef _DEV_CONFIG_IDF_H_
#define _DEV_CONFIG_IDF_H_

#include "driver/gpio.h"
#include <stdint.h>
#include "esp_err.h"

#define EPD_BUSY_PIN    CONFIG_EPD_BUSY_PIN
#define EPD_RST_PIN    	CONFIG_EPD_RST_PIN
#define EPD_DC_PIN      CONFIG_EPD_DC_PIN
#define EPD_CS_PIN      CONFIG_EPD_CS_PIN
#define EPD_SCK_PIN     CONFIG_EPD_SCK_PIN
#define EPD_MOSI_PIN    CONFIG_EPD_MOSI_PIN

esp_err_t DEV_Module_Init(void);
void DEV_GPIO_Init(void);
esp_err_t DEV_SPI_Init(void);
void DEV_SPI_WriteByte(uint8_t data);
void DEV_SPI_Write_nByte(uint8_t *data, uint16_t len);
void DEV_Digital_Write(gpio_num_t pin, uint8_t value);
uint8_t DEV_Digital_Read(gpio_num_t pin);
void DEV_Delay_ms(uint32_t xms);

#endif
