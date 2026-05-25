/*
 * ssd1306.c
 */

#include <string.h>
#include "esp_log.h"
#include "setup.h"
#include "ssd1306.h"

/* --- SSD1306 command definitions --- */
#define SSD1306_DISPLAYON              0xAF  ///< Set display ON
#define SSD1306_DISPLAYOFF             0xAE  ///< Set display OFF
#define SSD1306_MULTIPLEX              0xA8  ///< Set multiplex ratio
#define SSD1306_MEMORYMODE             0x20  ///< Set memory addressing mode
#define SSD1306_STARTLINE              0x40  ///< Set display start line
#define SSD1306_OFFSET                 0xD3  ///< Set display offset
#define SSD1306_SEGMENTREMAP           0xA1  ///< Set segment re-map (column address 127 is mapped to SEG0)
#define SSD1306_COMSCAN                0xC8  ///< Set COM output scan direction (remapped mode)
#define SSD1306_COMPINS                0xDA  ///< Set COM pins hardware configuration
#define SSD1306_CONTRAST               0x81  ///< Set contrast control
#define SSD1306_DISPLAYALLONRESUME     0xA4  ///< Entire display ON (resume to RAM content display)
#define SSD1306_INVERTON               0xA7  ///< Set inverted display
#define SSD1306_INVERTOFF              0xA6  ///< Set normal display (not inverted)
#define SSD1306_DISPLAYCLOCKDIV        0xD5  ///< Set display clock divide ratio/oscillator frequency
#define SSD1306_PRECHARGE              0xD9  ///< Set pre-charge period
#define SSD1306_VCOMH                  0xDB  ///< Set VCOMH deselect level
#define SSD1306_CHARGEPUMP             0x8D  ///< Set charge pump
#define SSD1306_DEACT_SCROLL           0x2E  ///< Deactivate scroll

#define SSD1306_COLUMNADDR             0x21  ///< Set column address: 0 to (width-1)
#define SSD1306_PAGEADDR               0x22  ///< Set page address: 0 to (pages-1)

static const char *TAG = "ssd1306";

/* -------- Attach device to I2C bus ------- */
esp_err_t ssd1306_attach(i2c_master_bus_handle_t bus, ssd1306_t *dev, uint8_t addr)
{

    i2c_device_config_t config = {
        .device_address = addr,
        .scl_speed_hz = 400000
    };
    esp_err_t ret;
    ret = i2c_master_bus_add_device(bus, &config, &dev->handle);
    if (ret == ESP_OK) {
        dev->address = addr;
        ESP_LOGI(TAG, "new device added to I2C bus at address 0x%02X", addr);
    } else {
        ESP_LOGE(TAG, "failed to add new device: %s", esp_err_to_name(ret));
    }
    return ret;
}

/* ------------- Write command ------------- */
static esp_err_t ssd1306_write_command(ssd1306_t *dev, uint8_t command)
{
    uint8_t data[2] = {0x00, command}; // Control byte 0x00 for commands
    return i2c_master_transmit(dev->handle, data, sizeof(data), -1);
}

/* -------------- Write data --------------- */
static esp_err_t ssd1306_write_data(ssd1306_t *dev, const uint8_t *buffer, size_t len)
{
    uint8_t data[len+1];
    data[0] = 0x40; // Control byte 0x40 for data
    memcpy(&data[1], buffer, len);
    return i2c_master_transmit(dev->handle, data, sizeof(data), -1);
}

/* -------- Initialization sequence -------- */
esp_err_t ssd1306_init(ssd1306_t *dev, uint8_t width, uint8_t height, ssd1306_mode_t mode)
{
    dev->width = width;
    dev->height = height;
    dev->mode = mode;

    // 0x1F - 32MUX for 128x32 display / 0x3F - 64MUX for 128x64 display
    uint8_t mux = (height == 32) ? 0x1F : 0x3F;
    // 0x02 for 128x32 display, 0x12 for 128x64 display
    uint8_t compins = (height == 32) ? 0x02 : 0x12;

    uint8_t init_sequence[] = {
        SSD1306_DISPLAYOFF,            // Display off
        SSD1306_MULTIPLEX,             // Set multiplex ratio:
        mux,                           // 0x1F - 32MUX for 128x32 display / 0x3F - 64MUX for 128x64 display
        SSD1306_MEMORYMODE,            // Memory addressing mode:
        (uint8_t)dev->mode,            // 0x00 - horizontal / 0x01 - vertical / 0x02 - page addressing mode
        SSD1306_STARTLINE | 0x00,      // Set start line to 0
        SSD1306_OFFSET,                // Set display offset:
        0x00,                          // No offset
        SSD1306_SEGMENTREMAP,          // Set segment re-map (column address 127 is mapped to SEG0)
        SSD1306_COMSCAN,               // Set COM output scan direction (remapped mode)
        SSD1306_COMPINS,               // Set COM pins hardware configuration:
        compins,                       // 0x02 for 128x32 display, 0x12 for 128x64 display
        SSD1306_CONTRAST,              // Set contrast control:
        0x7F,                          // Contrast value (0x00 to 0xFF)
        SSD1306_DISPLAYALLONRESUME,    // Entire display ON (resume to RAM content display)
        SSD1306_INVERTOFF,             // Set normal display (not inverted)
        SSD1306_DISPLAYCLOCKDIV,       // Set display clock divide ratio/oscillator frequency:
        0x80,                          // 0x80 => D=1; DCLK = Fosc / D <=> DCLK = Fosc
        SSD1306_PRECHARGE,             // Set pre-charge period:
        0xF1,                          // Pre-charge period (0x00 to 0xFF)
        SSD1306_VCOMH,                 // Set VCOMH deselect level:
        0x20,                          // 0x00 - 0.65xVcc, 0x20 - 0.77xVcc, 0x40 - 0.83xVcc
        SSD1306_CHARGEPUMP,            // Charge pump setting:
        0x14,                          // Enable charge pump
        SSD1306_DEACT_SCROLL,          // Deactivate scroll
        SSD1306_DISPLAYON              // Display ON
    };
    esp_err_t ret;
    for (size_t i = 0; i < sizeof(init_sequence); i++) {
        ret = ssd1306_write_command(dev, init_sequence[i]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "failed to write command in init sequence 0x%02X", init_sequence[i]);
            return ret;
        }
    }
    ESP_LOGI(TAG, "initialization commands sent successfully");
    return ESP_OK;
}

esp_err_t ssd1306_set_mode(ssd1306_t *dev, ssd1306_mode_t mode)
{
    if (dev->mode == mode) return ESP_OK;
    esp_err_t ret;
    uint8_t arg[] = {
        SSD1306_MEMORYMODE,
        (uint8_t)mode
    };
    for (size_t i = 0; i < sizeof(arg); i++) {
        ret = ssd1306_write_command(dev, arg[i]);
        if (ret != ESP_OK) return ret;
    }
    dev->mode = mode;
    ESP_LOGI(TAG, "memory mode changed to 0x%02X", (uint8_t)mode);
    return ESP_OK;
}

esp_err_t ssd1306_goto(ssd1306_t *dev, uint8_t col, uint8_t page)
{
    if (col >= dev->width || page >= (dev->height/8)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (dev->mode != SSD1306_MODE_PAGE) {
        ESP_LOGW(TAG, "function ssd1306_goto is not supported in current mode: 0x%02X",
                (uint8_t)dev->mode);
        return ESP_ERR_NOT_SUPPORTED;
    }
    esp_err_t ret;
    uint8_t arg[] = {
        (uint8_t)(0xB0 | page),
        (uint8_t)(0x00 | (col & 0x0F)),
        (uint8_t)(0x10 | ((col >> 4) & 0x0F))
    };
    for (size_t i = 0; i < sizeof(arg); i++) {
        ret = ssd1306_write_command(dev, arg[i]);
        if (ret != ESP_OK) return ret;
    }
    return ESP_OK;
}

esp_err_t ssd1306_set_window(ssd1306_t *dev, uint8_t start_col, uint8_t start_page, uint8_t end_col, uint8_t end_page)
{
    if (end_col >= dev->width || end_page >= dev->height/8 || start_col > end_col || start_page > end_page) {
        return ESP_ERR_INVALID_ARG;
    }
    if (dev->mode == SSD1306_MODE_PAGE) {
        ESP_LOGW(TAG, "function ssd1306_set_window is not supported in current mode: 0x%02X",
                (uint8_t)dev->mode);
        return ESP_ERR_NOT_SUPPORTED;
    }
    esp_err_t ret;
    uint8_t arg[] = {
        SSD1306_COLUMNADDR, start_col,  end_col,
        SSD1306_PAGEADDR,   start_page, end_page
    };
    for (size_t i = 0; i < sizeof(arg); i++) {
        ret = ssd1306_write_command(dev, arg[i]);
        if (ret != ESP_OK) return ret;
    }
    return ESP_OK;
}

esp_err_t ssd1306_clear(ssd1306_t *dev)
{
    esp_err_t ret;

    // Clear display in Page mode
    if (dev->mode == SSD1306_MODE_PAGE) {
        uint8_t nullpage[dev->width + 1];  // +1 for control byte
        memset(nullpage, 0, sizeof(nullpage));
        nullpage[0] = 0x40;  // control byte: data

        for (uint8_t page = 0; page < dev->height/8; page++) {
            ret = ssd1306_goto(dev, 0, page);
            if (ret != ESP_OK) return ret;
            ret = i2c_master_transmit(dev->handle, nullpage, sizeof(nullpage), -1);
            if (ret != ESP_OK) return ret;
        }
    
    // Clear display in Horizontal / Vertical mode
    } else {
        ret = ssd1306_set_window(dev, 0, 0, dev->width-1, (dev->height/8)-1);
        if (ret != ESP_OK) return ret;
        size_t buffer_size = dev->width * (dev->height/8) + 1;  // +1 for control byte
        uint8_t nullbuffer[buffer_size];
        memset(nullbuffer, 0, buffer_size);
        nullbuffer[0] = 0x40;  // control byte: data

        ret = i2c_master_transmit(dev->handle, nullbuffer, buffer_size, -1);
        if (ret != ESP_OK) return ret;
    }
    return ESP_OK;
}
