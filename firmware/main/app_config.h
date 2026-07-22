#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stdint.h>
#include "esp_err.h"

#define CONFIG_NAMESPACE "app_settings"

#define MAX_WIFI_NETWORKS 5

typedef struct {
    char ssid[32];
    char password[64];
} wifi_ap_t;

/**
 * @brief Application configuration structure
 * This can be expanded with more fields (like Wi-Fi settings, telemetry intervals, etc.)
 */
typedef struct {
    char udp_ip[16];
    uint16_t udp_port;
    wifi_ap_t wifi_aps[MAX_WIFI_NETWORKS];
    uint8_t wifi_ap_count;
    float voltage_coef;
    float voltage_offset;
    float current_coef;
    float current_offset;
} app_config_t;

/**
 * @brief Initialize the configuration manager, load values from NVS or fall back to defaults
 * @return esp_err_t ESP_OK on success
 */
esp_err_t app_config_init(void);

/**
 * @brief Get a thread-safe copy of the current configuration
 * @param[out] config Pointer to structure destination
 */
void app_config_get(app_config_t *config);

/**
 * @brief Thread-safe update of the configuration, and persist to NVS
 * @param[in] config Pointer to the source structure with new settings
 * @return esp_err_t ESP_OK on success
 */
esp_err_t app_config_set(const app_config_t *config);

/**
 * @brief Reset the active configuration and erase it from NVS.
 * 
 * @return 
 * - ESP_OK             NVS namespace erased and committed successfully.
 * - ESP_ERR_INVALID_STATE Config subsystem is not initialized.
 * - Others             NVS API errors.
 */
esp_err_t app_config_factory_reset(void);

#endif // APP_CONFIG_H
