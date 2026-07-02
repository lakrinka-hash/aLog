/**
 * @file ads1115_drv.h
 * @brief ADS1115 16-bit ADC driver API (I2C)
 */

#ifndef ADS1115_DRV_H
#define ADS1115_DRV_H

#include <stdint.h>             // for uint8_t
#include "driver/i2c_master.h"  // for i2c_master_dev_handle_t
#include "esp_err.h"            // for esp_err_t

#define I2C_TIMEOUT_MS    100   // Ticks or ms depending on version

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @typedef ads1115_pga_t
 * @brief ADS1115 Programmable Gain Amplifier (PGA) options
 * @note These values correspond to bits 11-9 in the Config Register
 */
typedef enum {
    ADS1115_PGA_6144mV = 0x0000,  // ±6.144V
    ADS1115_PGA_4096mV = 0x0200,  // ±4.096V (Default)
    ADS1115_PGA_2048mV = 0x0400,  // ±2.048V
    ADS1115_PGA_1024mV = 0x0600,  // ±1.024V
    ADS1115_PGA_0512mV = 0x0800,  // ±0.512V
    ADS1115_PGA_0256mV = 0x0A00   // ±0.256V
} ads1115_pga_t;

/**
 * @typedef ads1115_rate_t
 * @brief ADS1115 Data Rate (SPS - Samples Per Second)
 * @note These values correspond to bits 7-5 in the Config Register
 */
typedef enum {
    ADS1115_RATE_8SPS   = 0x0000,  // 8 samples per second
    ADS1115_RATE_16SPS  = 0x0020,  // 16 samples per second
    ADS1115_RATE_32SPS  = 0x0040,  // 32 samples per second
    ADS1115_RATE_64SPS  = 0x0060,  // 64 samples per second
    ADS1115_RATE_128SPS = 0x0080,  // 128 samples per second (default)
    ADS1115_RATE_250SPS = 0x00A0,  // 250 samples per second
    ADS1115_RATE_475SPS = 0x00C0,  // 475 samples per second
    ADS1115_RATE_860SPS = 0x00E0   // 860 samples per second
} ads1115_rate_t;

/**
 * @typedef ads1115_mode_t
 * @brief ADS1115 Operating Mode
 * @note This value corresponds to bit 8 in the Config Register
 */
typedef enum {
    ADS1115_MODE_CONTINUOUS = 0x0000,
    ADS1115_MODE_SINGLE_SHOT = 0x0100
} ads1115_mode_t;

/**
 * @struct ads1115_t
 * @brief ADS1115 Device configuration and handle structure
 */
typedef struct {
    i2c_master_dev_handle_t handle;   //!< I2C master driver device handle
    uint8_t address;                  //!< I2C device address
    uint8_t channel;                  //!< Current active channel (0..3)
    ads1115_pga_t pga;                //!< Current PGA range setting
    ads1115_rate_t rate;              //!< Current data rate setting
    ads1115_mode_t mode;              //!< Current operating mode
} ads1115_t;

/* ========================================================================= */
/*                           PUBLIC API PROTOTYPES                           */
/* ========================================================================= */

/**
 * @brief Attach the ADS1115 device to the initialized I2C master bus
 * @param[in]  bus  I2C bus handle
 * @param[out] dev  Pointer to the device structure
 * @param[in]  addr I2C address of the ADS1115
 * @return
 * - ESP_OK on success
 * - ESP_ERR_INVALID_ARG if bus or dev pointer is null
 * - I2C driver error code on failure.
 */
esp_err_t ads1115_attach(i2c_master_bus_handle_t bus, ads1115_t *dev, uint8_t addr);

/**
 * @brief Set the operating mode (Single-Shot or Continuous)
 * @param[in,out] dev Pointer to the device structure
 * @param[in]    mode Mode selection from ads1115_mode_t
 */
void ads1115_set_mode(ads1115_t *dev, ads1115_mode_t mode);

/**
 * @brief Set the active channel for the ADS1115
 * @param[in,out] dev Pointer to the device structure
 * @param[in] channel Channel number (0-3 for AIN0-AIN3)
 * @return
 * - ESP_OK if successful
 * - ESP_ERR_INVALID_ARG if out of bounds.
 */
esp_err_t ads1115_set_channel(ads1115_t *dev, uint8_t channel);

/**
 * @brief Set the programmable gain amplifier (PGA) range
 * @param[in,out] dev Pointer to the device structure
 * @param[in]     pga PGA range selection from ads1115_pga_t
 */
void ads1115_set_pga(ads1115_t *dev, ads1115_pga_t pga);

/**
 * @brief Set the output data rate (SPS)
 * @param[in,out] dev  Pointer to the device structure
 * @param[in]    rate Data rate selection from ads1115_rate_t
 */
void ads1115_set_rate(ads1115_t *dev, ads1115_rate_t rate);

/**
 * @brief Transmit the configuration register settings to start conversion
 * @param[in] dev Pointer to the device structure
 * @return ESP_OK on success or I2C driver error code on failure
 */
esp_err_t ads1115_start_conversion(ads1115_t *dev);

/**
 * @brief Read raw 16-bit signed conversion value directly
 * @param[in]  dev   Pointer to the device structure
 * @param[out] value Pointer to store the 16-bit raw signed integer
 * @return
 * - ESP_OK on success
 * - ESP_ERR_INVALID_ARG if value pointer is NULL
 * - I2C driver error code on failure.
 */
esp_err_t ads1115_read_raw(ads1115_t *dev, int16_t *value);

/**
 * @brief Read and normalize the conversion value to a ratio between -1.0 and +1.0
 * @param[in]  dev   Pointer to the device structure
 * @param[out] value Pointer to store the normalized float value
 * @return
 * - ESP_OK on success
 * - ESP_ERR_INVALID_ARG if value pointer is NULL
 * - I2C driver error code on failure.
 */
esp_err_t ads1115_get_normalized(ads1115_t *dev, float *value);

/**
 * @brief Read and calculate the voltage based on current PGA settings
 * @param[in]  dev     Pointer to the device structure
 * @param[out] voltage Pointer to store the calculated voltage in Volts
 * @return
 * - ESP_OK on success
 * - ESP_ERR_INVALID_ARG if voltage pointer is NULL
 * - I2C driver error code on failure.
 */
esp_err_t ads1115_get_voltage(ads1115_t *dev, float *voltage);

#ifdef __cplusplus
}
#endif

#endif /* ADS1115_DRV_H */