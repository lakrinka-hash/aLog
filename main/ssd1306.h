/**
 * @file ssd1306.h
 * @brief SSD1306 OLED display driver API (I²C).
 */

#ifndef SSD1306_H
#define SSD1306_H

#include "esp_err.h"
#include "driver/i2c_master.h"

/**
 * @brief SSD1306 memory addressing modes
 */
typedef enum {
    SSD1306_MODE_HORIZONTAL = 0x00,  ///< Horizontal addressing mode
    SSD1306_MODE_VERTICAL   = 0x01,  ///< Vertical addressing mode
    SSD1306_MODE_PAGE       = 0x02   ///< Page addressing mode
} ssd1306_mode_t;

/**
 * @struct ssd1306_t
 * @brief SSD1306 device configuration and state structure
 */
typedef struct {
    i2c_master_dev_handle_t handle;   ///< I2C device handle
    uint8_t address;                  ///< I2C address of the SSD1306
    uint8_t width;                    ///< Display width in pixels
    uint8_t height;                   ///< Display height in pixels
    ssd1306_mode_t mode;              ///< Current memory addressing mode
} ssd1306_t;

/**
 * @brief Attach SSD1306 device to I2C bus
 * @param bus I2C master bus handle
 * @param dev SSD1306 device structure pointer
 * @param addr I2C address of the SSD1306 display
 * @return ESP_OK on success, or I2C driver error code on failure
 */
esp_err_t ssd1306_attach(i2c_master_bus_handle_t bus, ssd1306_t *dev, uint8_t addr);

/**
 * @brief Initialize SSD1306 device with specified parameters
 * @param dev SSD1306 device structure pointer
 * @param width Display width in pixels (e.g., 128)
 * @param height Display height in pixels (e.g., 32 or 64)
 * @param mode Desired memory addressing mode
 * @return ESP_OK on success, error code on failure
 */
esp_err_t ssd1306_init(ssd1306_t *dev, uint8_t width, uint8_t height, ssd1306_mode_t mode);

/**
 * @brief Change the memory addressing mode of the display on the fly
 * @note This function automatically clears the display after a successful mode switch
 * to prevent visual corruption.
 * @param dev SSD1306 device structure pointer
 * @param mode The desired memory addressing mode (Page, Horizontal, Vertical)
 * @return ESP_OK on success, or error code on failure
 */
esp_err_t ssd1306_set_mode(ssd1306_t *dev, ssd1306_mode_t mode);

#endif /* SSD1306_H */