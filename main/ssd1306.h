/**
 * @file ssd1306.h
 * @brief SSD1306 OLED display driver API (I²C).
 */

#ifndef SSD1306_H
#define SSD1306_H

#include "esp_err.h"
#include "driver/i2c_master.h"

/**
 * @struct ssd1306_t
 * @brief SSD1306 device structure
 */
typedef struct {
    i2c_master_dev_handle_t handle;   ///< I2C device handle
    uint8_t address;                  ///< I2C address of the SSD1306
    uint8_t width;                    ///< Display width in pixels
    uint8_t height;                   ///< Display height in pixels
} ssd1306_t;

/**
 * @brief Attach SSD1306 device to I2C bus
 * @param bus I2C bus handle
 * @param dev SSD1306 device structure
 * @param addr I2C address of the SSD1306
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t ssd1306_attach(i2c_master_bus_handle_t bus, ssd1306_t *dev, uint8_t addr);

/**
 * @brief Initialize SSD1306 device
 * @param dev SSD1306 device structure
 * @param width Display width in pixels
 * @param height Display height in pixels
 * @return ESP_OK on success, error code on failure
 */
esp_err_t ssd1306_init(ssd1306_t *dev, uint8_t width, uint8_t height);

#endif /* SSD1306_H */