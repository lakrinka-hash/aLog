/*
 * setup.h
 */

#ifndef SETUP_H
#define SETUP_H

#include "driver/gpio.h" // IWYU pragma: keep

// I2C bus
#define I2C_SDA             GPIO_NUM_10
#define I2C_SCL             GPIO_NUM_9

// SPI bus
#define SPI_MISO            GPIO_NUM_5
#define SPI_MOSI            GPIO_NUM_7
#define SPI_SCK             GPIO_NUM_6
#define SPI_CS              GPIO_NUM_4

// SSD1306
#define SSD1306_WIDTH       128
#define SSD1306_HEIGHT      32
#define SSD1306_ADDR        0x3C

#endif /* SETUP_H */
