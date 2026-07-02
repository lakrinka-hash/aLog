/**
 * @file ssd1306_drv.h
 * @brief SSD1306 OLED display driver API (I²C)
 *
 * Provides a low-level, hardware-specific transport layer for configuring,
 * positioning, and streaming raw data to the SSD1306 dot-matrix controller
 * @note This driver is explicitly designed for the modern ESP-IDF v5.3+ / v6.0+
 * I2C Master API (`driver/i2c_master.h`) and utilizes scatter-gather
 * multi-buffer transmissions. It is not compatible with legacy `driver/i2c.h`
 */

#ifndef SSD1306_DRV_H
#define SSD1306_DRV_H

#include <stdint.h>            // for uint8_t
#include <stddef.h>            // for size_t
#include "esp_err.h"           // for esp_err_t
#include "driver/i2c_types.h"  // for i2c_master_dev_handle_t, i2c_master_bus_handle_t
#include "fonts.h"             // for font_mono_t

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
    const font_mono_t *font;          ///< Current font pointer
} ssd1306_t;

/**
 * @brief Attach SSD1306 device to I2C bus
 *
 * @param[in]  bus  I2C master bus handle
 * @param[out] dev  SSD1306 device structure pointer
 * @param[in] addr  I2C address of the SSD1306 display
 *
 * @return
 * - ESP_OK on success
 * - ESP_ERR_INVALID_ARG if bus or dev pointer is null
 * - I2C driver error code on failure
 */
esp_err_t ssd1306_attach(i2c_master_bus_handle_t bus, ssd1306_t *dev, uint8_t addr);

/**
 * @brief Initialize SSD1306 device with specified parameters
 *
 * @param[in,out] dev  SSD1306 device structure pointer
 * @param[in]   width  Display width in pixels (e.g., 128)
 * @param[in]  height  Display height in pixels (e.g., 32 or 64)
 * @param[in]    mode  Desired memory addressing mode
 *
 * @return
 * - ESP_OK on success
 * - I2C driver error code on failure
 */
esp_err_t ssd1306_init(ssd1306_t *dev, uint8_t width, uint8_t height, ssd1306_mode_t mode);

/**
 * @brief Change the memory addressing mode of the display on the fly
 *
 * @param[in,out] dev  SSD1306 device structure pointer
 * @param[in]    mode  The desired memory addressing mode (Page, Horizontal, Vertical)
 *
 * @return
 * - ESP_OK on success
 * - I2C driver error code on failure
 */
esp_err_t ssd1306_set_mode(ssd1306_t *dev, ssd1306_mode_t mode);

/**
 * @brief Set GDDRAM cursor position (Supported in Page Mode only)
 *
 * @param[in,out] dev  SSD1306 device structure pointer
 * @param[in]     col  Column address (0 to width-1)
 * @param[in]    page  Page address (0 to height/8 - 1)
 *
 * @return 
 * - ESP_OK on success
 * - ESP_ERR_INVALID_ARG if coordinates are out of bounds
 * - ESP_ERR_NOT_SUPPORTED if called in Horizontal/Vertical modes
 * - I2C driver error code on failure
 */
esp_err_t ssd1306_goto(ssd1306_t *dev, uint8_t col, uint8_t page);

/**
 * @brief Set hardware display window boundaries (Supported in Horizontal/Vertical modes only)
 *
 * @param[in,out]    dev  SSD1306 device structure pointer
 * @param[in]  start_col  Start column address (0 to width-1)
 * @param[in] start_page  Start page address (0 to height/8 - 1)
 * @param[in]    end_col  End column address (start_col to width-1)
 * @param[in]   end_page  End page address (start_page to height/8 - 1)
 *
 * @return 
 * - ESP_OK on success
 * - ESP_ERR_INVALID_ARG if boundaries are invalid or out of bounds
 * - ESP_ERR_NOT_SUPPORTED if called in Page Mode
 * - I2C driver error code on failure
 */
esp_err_t ssd1306_set_window(ssd1306_t *dev, uint8_t start_col, uint8_t start_page, uint8_t end_col, uint8_t end_page);

/**
 * @brief Clear the entire display (Fills GDDRAM with 0x00)
 * @note Automatically adapts the clearing method based on the current dev->mode
 *
 * @param[in,out] dev  SSD1306 device structure pointer
 *
 * @return
 * - ESP_OK on success
 * - I2C driver error code on failure
 */
esp_err_t ssd1306_clear(ssd1306_t *dev);

/**
 * @brief Set the active font for the SSD1306 device
 *
 * @param[in,out] dev  SSD1306 device structure pointer
 * @param[in]    font  Pointer to the monochromatic font descriptor structure
 *
 * @return
 * - ESP_OK on success
 * - ESP_ERR_INVALID_ARG if font is invalid
 */
esp_err_t ssd1306_set_font(ssd1306_t *dev, const font_mono_t *font);

/**
 * @brief Draw a single character on the SSD1306 display
 *
 * @param[in,out] dev  SSD1306 device structure pointer
 * @param[in]       c  Character to draw
 *
 * @return 
 * - ESP_OK on success
 * - ESP_ERR_INVALID_ARG if configuration is invalid
 * - I2C driver error code on failure
 */
esp_err_t ssd1306_draw_char(ssd1306_t *dev, char c);

/**
 * @brief Draw a string on the SSD1306 display.
 *
 * @param[in,out] dev  SSD1306 device structure pointer
 * @param[in]     str  Null-terminated string to draw
 *
 * @return
 * - ESP_OK on success
 * - ESP_ERR_INVALID_ARG if inputs are invalid
 * - I2C driver error code on failure
 */
esp_err_t ssd1306_draw_string(ssd1306_t *dev, const char *str);

/**
 * @brief Transmit a raw byte stream directly to the GDDRAM using scatter-gather multi-buffering
 * @note This function automatically appends the 0x40 Data Control Byte in front of the stream
 * without copying the main buffer payload, ensuring zero-copy efficiency
 *
 * @param[in,out] dev  SSD1306 device structure pointer
 * @param[in]  buffer  Pointer to the data payload to stream
 * @param[in]    size  Size of the data payload in bytes
 *
 * @return
 * - ESP_OK on success
 * - I2C driver error code on failure
 */
esp_err_t ssd1306_write_stream(ssd1306_t *dev, const uint8_t *buffer, size_t size);

#endif /* SSD1306_DRV_H */
