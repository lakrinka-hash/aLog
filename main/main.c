/*
 * main.c
 */

#include <stdio.h>
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "setup.h"

static const char *TAG = "main";

i2c_master_bus_handle_t i2c_bus_handle;

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

/* ------------- APP Main ------------- */
void app_main(void)
{
    i2c_bus_init();
}
