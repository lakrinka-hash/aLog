/*
 * main.c
 */

#include <stdio.h>
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "setup.h"
#include "ssd1306.h"
#include "fonts.h"

static const char *TAG = "main";

i2c_master_bus_handle_t i2c_bus_handle;
ssd1306_t lcd;

/* ------ I2C bus initialization ------ */
void i2c_bus_init(void)
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_SDA,
        .scl_io_num = I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .flags.enable_internal_pullup = true
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &i2c_bus_handle));
    ESP_LOGI(TAG, "I2C bus initialized on port %d",
            bus_config.i2c_port);
    ESP_LOGI(TAG, "I2C bus pin: SDA=%d, SCL=%d",
            bus_config.sda_io_num,
            bus_config.scl_io_num);
}

/* ------ SPI bus initialization ------ */
void spi_bus_init(void)
{
    spi_bus_config_t bus_config = {
        .mosi_io_num = SPI_MOSI,
        .miso_io_num = SPI_MISO,
        .sclk_io_num = SPI_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus_config, SPI_DMA_CH_AUTO));
    ESP_LOGI(TAG, "SPI bus initialized on host SPI2");
    ESP_LOGI(TAG, "SPI bus pin: MISO=%d, MOSI=%d, SCK=%d",
            bus_config.miso_io_num,
            bus_config.mosi_io_num,
            bus_config.sclk_io_num);
}

/* ------------- APP Main ------------- */
void app_main(void)
{
    esp_err_t ret;
    i2c_bus_init();
    ret = ssd1306_attach(i2c_bus_handle, &lcd, SSD1306_ADDR);
    if (ret == ESP_OK) ssd1306_init(&lcd, SSD1306_WIDTH, SSD1306_HEIGHT, SSD1306_MODE_PAGE);
    else return;
    spi_bus_init();
    ssd1306_set_font(&lcd, &font8x8uk);
}
