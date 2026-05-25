/*
 * ssd1306.c
 */

#include "esp_log.h"
#include "setup.h"
#include "ssd1306.h"

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

