/**
 * @file ds1307_drv.h
 * @brief DS1307 Real-Time Clock (RTC) driver API for ESP-IDF
 */

#ifndef DS1307_DRV_H
#define DS1307_DRV_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>               // for struct tm
#include "driver/i2c_master.h"  // for i2c_master_dev_handle_t
#include "esp_err.h"            // for esp_err_t

#define DS1307_I2C_ADDR    0x68
#define DS1307_TIMEOUT_MS  500

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @struct ds1307_t
 * @brief DS1307 Device configuration and handle structure
 */
typedef struct {
    i2c_master_dev_handle_t handle; //!< I2C master driver device handle
    uint8_t address;                //!< I2C device address (normally 0x68)
} ds1307_t;

/**
 * @brief Attach the DS1307 RTC device to the initialized I2C master bus.
 * 
 * @param[in]  bus  I2C master bus handle
 * @param[out] dev  Pointer to the device structure
 * @param[in]  addr I2C address of the DS1307 (usually DS1307_I2C_ADDR)
 * @return
 * - ESP_OK on success
 * - ESP_ERR_INVALID_ARG if bus or dev pointer is null
 * - I2C driver error code on failure.
 */
esp_err_t ds1307_attach(i2c_master_bus_handle_t bus, ds1307_t *dev, uint8_t addr);

/**
 * @brief Set the time in the DS1307 RTC from a standard POSIX struct tm.
 * @note This function will automatically start the clock oscillator by clearing the CH bit.
 * 
 * @param[in] dev Pointer to the DS1307 device structure
 * @param[in] tm  Pointer to the struct tm containing the time to write
 * @return ESP_OK on success, or an ESP-IDF error code.
 */
esp_err_t ds1307_set_time(const ds1307_t *dev, const struct tm *tm);

/**
 * @brief Get the current time from the DS1307 RTC as a standard POSIX struct tm.
 * 
 * @param[in]  dev Pointer to the DS1307 device structure
 * @param[out] tm  Pointer to the struct tm to fill with the read time
 * @return ESP_OK on success, or an ESP-IDF error code.
 */
esp_err_t ds1307_get_time(const ds1307_t *dev, struct tm *tm);

/**
 * @brief Check if the DS1307 clock oscillator is halted (CH bit is set).
 * 
 * @param[in]  dev    Pointer to the DS1307 device structure
 * @param[out] halted Pointer to a boolean to store the halt status (true if halted)
 * @return ESP_OK on success, or an ESP-IDF error code.
 */
esp_err_t ds1307_is_halted(const ds1307_t *dev, bool *halted);

/**
 * @brief Clear the clock halt (CH) bit to start the oscillator if it was stopped.
 * 
 * @param[in] dev Pointer to the DS1307 device structure
 * @return ESP_OK on success, or an ESP-IDF error code.
 */
esp_err_t ds1307_start_oscillator(const ds1307_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* DS1307_DRV_H */
