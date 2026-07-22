/**
 * @file ds1307_drv.c
 * @brief DS1307 Real-Time Clock (RTC) driver implementation
 */

#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "ds1307_drv.h"

static const char *TAG = "ds1307_drv";

#define REG_SECONDS 0x00
#define REG_MINUTES 0x01
#define REG_HOURS   0x02
#define REG_DAY     0x03
#define REG_DATE    0x04
#define REG_MONTH   0x05
#define REG_YEAR    0x06
#define REG_CONTROL 0x07

static inline uint8_t dec_to_bcd(uint8_t dec)
{
    return ((dec / 10) << 4) | (dec % 10);
}

static inline uint8_t bcd_to_dec(uint8_t bcd)
{
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

esp_err_t ds1307_attach(i2c_master_bus_handle_t bus, ds1307_t *dev, uint8_t addr)
{
    if (!dev || !bus) return ESP_ERR_INVALID_ARG;

    i2c_device_config_t config = {
        .device_address = addr,
        .scl_speed_hz = 100000 // DS1307 runs at 100kHz standard I2C speed
    };

    esp_err_t ret = i2c_master_bus_add_device(bus, &config, &dev->handle);
    if (ret == ESP_OK) {
        dev->address = addr;
        ESP_LOGI(TAG, "DS1307 attached to I2C bus at address 0x%02X", addr);
    } else {
        ESP_LOGE(TAG, "Failed to attach DS1307 device: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t ds1307_set_time(const ds1307_t *dev, const struct tm *tm)
{
    if (!dev || !tm) return ESP_ERR_INVALID_ARG;

    uint8_t write_buf[8];
    write_buf[0] = REG_SECONDS;
    
    // Clear Clock Halt (CH) bit 7 to ensure the oscillator runs
    write_buf[1] = dec_to_bcd(tm->tm_sec) & 0x7F;
    write_buf[2] = dec_to_bcd(tm->tm_min) & 0x7F;
    // Set 24h mode by clearing bit 6, and write BCD hours
    write_buf[3] = dec_to_bcd(tm->tm_hour) & 0x3F;
    // Day of week: 1-7
    write_buf[4] = dec_to_bcd(tm->tm_wday + 1) & 0x07;
    // Date: 1-31
    write_buf[5] = dec_to_bcd(tm->tm_mday) & 0x3F;
    // Month: 1-12
    write_buf[6] = dec_to_bcd(tm->tm_mon + 1) & 0x1F;
    // Year: 0-99 (tm_year is years since 1900, so year % 100 gives 0-99 since 2000)
    write_buf[7] = dec_to_bcd(tm->tm_year % 100);

    esp_err_t ret = i2c_master_transmit(dev->handle, write_buf, sizeof(write_buf), DS1307_TIMEOUT_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write time to DS1307: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGD(TAG, "DS1307 time set successfully");
    }
    return ret;
}

esp_err_t ds1307_get_time(const ds1307_t *dev, struct tm *tm)
{
    if (!dev || !tm) return ESP_ERR_INVALID_ARG;

    uint8_t reg = REG_SECONDS;
    uint8_t read_buf[7] = {0};

    // Read 7 timekeeping registers sequentially using single atomic transaction with Repeated START
    esp_err_t ret = i2c_master_transmit_receive(dev->handle, &reg, 1, read_buf, sizeof(read_buf), DS1307_TIMEOUT_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read time from DS1307: %s", esp_err_to_name(ret));
        return ret;
    }

    // Decode Seconds (bit 7 is CH bit, so mask with 0x7F)
    tm->tm_sec = bcd_to_dec(read_buf[0] & 0x7F);
    tm->tm_min = bcd_to_dec(read_buf[1] & 0x7F);

    // Decode Hours
    if (read_buf[2] & 0x40) { // 12-hour mode
        uint8_t hour = bcd_to_dec(read_buf[2] & 0x1F);
        if (read_buf[2] & 0x20) { // PM
            if (hour < 12) hour += 12;
        } else { // AM
            if (hour == 12) hour = 0;
        }
        tm->tm_hour = hour;
    } else { // 24-hour mode
        tm->tm_hour = bcd_to_dec(read_buf[2] & 0x3F);
    }

    // Decode Day of week (1-7), tm expects 0-6
    tm->tm_wday = bcd_to_dec(read_buf[3] & 0x07) - 1;
    if (tm->tm_wday < 0 || tm->tm_wday > 6) {
        tm->tm_wday = 0;
    }

    // Decode Date (1-31)
    tm->tm_mday = bcd_to_dec(read_buf[4] & 0x3F);

    // Decode Month (1-12), tm expects 0-11
    tm->tm_mon = bcd_to_dec(read_buf[5] & 0x1F) - 1;
    if (tm->tm_mon < 0 || tm->tm_mon > 11) {
        tm->tm_mon = 0;
    }

    // Decode Year (0-99), represents 2000-2099. tm expects years since 1900.
    // So 100 + year_from_2000.
    tm->tm_year = bcd_to_dec(read_buf[6]) + 100;
    tm->tm_isdst = -1; // Not known

    return ESP_OK;
}

esp_err_t ds1307_is_halted(const ds1307_t *dev, bool *halted)
{
    if (!dev || !halted) return ESP_ERR_INVALID_ARG;

    uint8_t reg = REG_SECONDS;
    uint8_t seconds_reg = 0;

    esp_err_t ret = i2c_master_transmit_receive(dev->handle, &reg, 1, &seconds_reg, 1, DS1307_TIMEOUT_MS);
    if (ret == ESP_OK) {
        *halted = (seconds_reg & 0x80) ? true : false;
    } else {
        ESP_LOGE(TAG, "Failed to read seconds/CH register: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t ds1307_start_oscillator(const ds1307_t *dev)
{
    if (!dev) return ESP_ERR_INVALID_ARG;

    uint8_t reg = REG_SECONDS;
    uint8_t seconds_reg = 0;

    esp_err_t ret = i2c_master_transmit_receive(dev->handle, &reg, 1, &seconds_reg, 1, DS1307_TIMEOUT_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read seconds register to clear CH: %s", esp_err_to_name(ret));
        return ret;
    }

    if (seconds_reg & 0x80) {
        ESP_LOGI(TAG, "Clock oscillator is currently halted. Enabling oscillator...");
        uint8_t write_buf[2] = { REG_SECONDS, (uint8_t)(seconds_reg & 0x7F) };
        ret = i2c_master_transmit(dev->handle, write_buf, sizeof(write_buf), DS1307_TIMEOUT_MS);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to enable oscillator: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "Clock oscillator started successfully");
        }
    } else {
        ESP_LOGI(TAG, "Clock oscillator is already running");
    }

    return ret;
}
