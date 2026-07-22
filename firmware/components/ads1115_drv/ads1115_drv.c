/**
 * @file ads1115_drv.c
 * @brief ADS1115 16-bit ADC driver implementation.
 */

#include <stddef.h>             // for size_t
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "ads1115_drv.h"

static const char *TAG = "ads1115";

#define ADS1115_CONVERSION_REG    0x00  // Conversion register
#define ADS1115_CONFIG_REG        0x01  // Configuration register

/******************** PGA Map ********************/
typedef struct {
    ads1115_pga_t pga;
    float voltage;
} pga_map_t;

static const pga_map_t pga_table[] = {
    { ADS1115_PGA_6144mV, 6.144f },
    { ADS1115_PGA_4096mV, 4.096f },
    { ADS1115_PGA_2048mV, 2.048f },
    { ADS1115_PGA_1024mV, 1.024f },
    { ADS1115_PGA_0512mV, 0.512f },
    { ADS1115_PGA_0256mV, 0.256f }
};
#define PGA_TABLE_SIZE (sizeof(pga_table) / sizeof(pga_table[0]))

static float pga_to_voltage(ads1115_pga_t pga) {
    for (size_t i = 0; i < PGA_TABLE_SIZE; i++) {
        if (pga_table[i].pga == pga) return pga_table[i].voltage;
    }
    return -1.0f;
}

/******************** Rate Map ********************/
typedef struct {
    ads1115_rate_t rate;
    uint16_t sps;
} rate_map_t;

static const rate_map_t rate_table[] = {
    { ADS1115_RATE_8SPS,   8   },
    { ADS1115_RATE_16SPS,  16  },
    { ADS1115_RATE_32SPS,  32  },
    { ADS1115_RATE_64SPS,  64  },
    { ADS1115_RATE_128SPS, 128 },
    { ADS1115_RATE_250SPS, 250 },
    { ADS1115_RATE_475SPS, 475 },
    { ADS1115_RATE_860SPS, 860 }
};
#define RATE_TABLE_SIZE (sizeof(rate_table) / sizeof(rate_table[0]))

static uint16_t rate_to_sps(ads1115_rate_t rate) {
    for (size_t i = 0; i < RATE_TABLE_SIZE; i++) {
        if (rate_table[i].rate == rate) return rate_table[i].sps;
    }
    return 0;
}

/************* Attach ADC to I2C Bus *************/
esp_err_t ads1115_attach(i2c_master_bus_handle_t bus, ads1115_t *dev, uint8_t addr)
{
    if (!dev || !bus) return ESP_ERR_INVALID_ARG;

    i2c_device_config_t config = {
        .device_address = addr,
        .scl_speed_hz = 400000
    };
    esp_err_t ret = i2c_master_bus_add_device(bus, &config, &dev->handle);
    if (ret == ESP_OK) {
        dev->address = addr;
        dev->channel = 0;
        dev->pga = ADS1115_PGA_4096mV;
        dev->rate = ADS1115_RATE_128SPS;
        dev->mode = ADS1115_MODE_SINGLE_SHOT;
        
        ESP_LOGI(TAG, "New device (handle=%p) added to I2C bus at address 0x%02X:", 
                (void*)dev->handle,
                addr);
        ESP_LOGI(TAG, "dev info ---> PGA=±%.3fV, Rate=%u SPS, Mode=%s",
                pga_to_voltage(dev->pga),
                rate_to_sps(dev->rate),
                (dev->mode == ADS1115_MODE_CONTINUOUS) ? "CONTINUOUS" : "SINGLE-SHOT");
    } else {
        ESP_LOGE(TAG, "Failed to add device to bus. err: %s", esp_err_to_name(ret));
    }
    return ret;
}

/******************** Set Mode ********************/
void ads1115_set_mode(ads1115_t *dev, ads1115_mode_t mode)
{
    if (!dev) return;
    dev->mode = mode;
    ESP_LOGI(TAG, "Device (handle=%p) mode changed to %s",
            (void*)dev->handle,
            (mode == ADS1115_MODE_CONTINUOUS) ? "CONTINUOUS" : "SINGLE-SHOT");
}

/****************** Set Channel ******************/
esp_err_t ads1115_set_channel(ads1115_t *dev, uint8_t channel)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    if (channel > 3) return ESP_ERR_INVALID_ARG;
    dev->channel = channel;
    return ESP_OK;
}

/******************** Set PGA ********************/
void ads1115_set_pga(ads1115_t *dev, ads1115_pga_t pga)
{
    if (!dev) return;
    float range = pga_to_voltage(pga);
    if (range < 0.0f) {
        ESP_LOGW(TAG, "Device (handle=%p): Invalid PGA setting 0x%04X",
                (void*)dev->handle,
                pga);
            return;
    }
    dev->pga = pga;
    ESP_LOGI(TAG, "Device (handle=%p) PGA updated to ±%.3fV (raw: 0x%04X)",
            (void*)dev->handle,
            range,
            pga);
}

/******************** Set Rate ********************/
void ads1115_set_rate(ads1115_t *dev, ads1115_rate_t rate)
{
    if (!dev) return;
    uint16_t sps = rate_to_sps(rate);
    if (sps == 0) {
        ESP_LOGW(TAG, "Device (handle=%p): Invalid data rate setting 0x%04X",
                (void*)dev->handle,
                rate);
            return;
    }
    dev->rate = rate;
    ESP_LOGI(TAG, "Device (handle=%p) data rate set %u SPS (raw: 0x%04X)",
            (void*)dev->handle,
            sps,
            rate);
}

/**************** Start Conversion ****************/
esp_err_t ads1115_start_conversion(ads1115_t *dev)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    uint16_t config_word = 0x8000                        // Operational status
                         | ((0x04 + dev->channel) << 12) // MUX: AIN0..AIN3 vs GND
                         | dev->pga                      // Gain (PGA)
                         | dev->mode                     // Operating mode
                         | dev->rate                     // Data rate (SPS)
                         | 0x0000;                       // Activation of the ALERT/RDY pin

    uint8_t config_buf[3] = {
        ADS1115_CONFIG_REG,
        (uint8_t)((config_word >> 8) & 0xFF),
        (uint8_t)(config_word & 0xFF)
    };
    return i2c_master_transmit(dev->handle, config_buf, sizeof(config_buf), I2C_TIMEOUT_MS);
}

/****************** Read Config *******************/
esp_err_t ads1115_read_config(ads1115_t *dev, uint16_t *config)
{
    if (!dev || !config) return ESP_ERR_INVALID_ARG;
    uint8_t reg = ADS1115_CONFIG_REG;
    uint8_t data[2] = {0};
    esp_err_t ret = i2c_master_transmit_receive(dev->handle, &reg, 1, data, 2, I2C_TIMEOUT_MS);
    if (ret == ESP_OK) {
        *config = (uint16_t)((data[0] << 8) | data[1]);
    }
    return ret;
}

/************* Data Ready Status Check ***********/
esp_err_t ads1115_data_ready(ads1115_t *dev, bool *ready)
{
    if (!dev || !ready) return ESP_ERR_INVALID_ARG;
    uint16_t config = 0;
    esp_err_t ret = ads1115_read_config(dev, &config);
    if (ret == ESP_OK) {
        // Bit 15 is OS bit: 1 means conversion is complete
        *ready = (config & 0x8000) ? true : false;
    }
    return ret;
}

/******************** Read Raw ********************/
esp_err_t ads1115_read_raw(ads1115_t *dev, int16_t *value)
{
    if (!dev || !value) return ESP_ERR_INVALID_ARG;

    uint8_t reg = ADS1115_CONVERSION_REG;
    uint8_t data[2] = {0};
    esp_err_t ret = i2c_master_transmit_receive(dev->handle, &reg, 1, data, 2, I2C_TIMEOUT_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Device (handle=%p): Failed to read conversion register. err: %s",
                (void*)dev->handle,
                esp_err_to_name(ret));
        return ret;
    }
    *value = (int16_t)((data[0]<<8) | data[1]);
    return ESP_OK;
}

/**************** Calculate value ****************/
esp_err_t ads1115_get_normalized(ads1115_t *dev, float *value)
{
    if (!dev || !value) return ESP_ERR_INVALID_ARG;

    int16_t raw_value = 0;
    esp_err_t ret = ads1115_read_raw(dev, &raw_value);
    if (ret != ESP_OK) return ret;
    *value = (float)raw_value/32767.0f;
    return ESP_OK;
}

/**************** Calculate Voltage ****************/
esp_err_t ads1115_get_voltage(ads1115_t *dev, float *voltage)
{
    if (!dev || !voltage) return ESP_ERR_INVALID_ARG;
    float norm = 0.0f;
    esp_err_t ret = ads1115_get_normalized(dev, &norm);
    if (ret != ESP_OK) return ret;
    *voltage = norm * pga_to_voltage(dev->pga);
    return ESP_OK;
}