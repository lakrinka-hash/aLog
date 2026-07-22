#include "app_config.h"
#include <string.h>
#include "esp_log.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"  // IWYU pragma: keep
#include "freertos/semphr.h"
#include "sdkconfig.h"

static const char *TAG = "app_config";

static app_config_t active_config;
static SemaphoreHandle_t config_mutex = NULL;
static bool initialized = false;

esp_err_t app_config_init(void)
{
    if (initialized) {
        return ESP_OK;
    }

    config_mutex = xSemaphoreCreateMutex();
    if (config_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create configuration mutex");
        return ESP_ERR_NO_MEM;
    }

    // Set fallback defaults first
    memset(&active_config, 0, sizeof(active_config));
    strlcpy(active_config.udp_ip, CONFIG_APP_UDP_TARGET_IP, sizeof(active_config.udp_ip));
    active_config.udp_port = CONFIG_APP_UDP_TARGET_PORT;
    
    // Seed default Wi-Fi AP from SDKConfig
    active_config.wifi_ap_count = 1;
    strlcpy(active_config.wifi_aps[0].ssid, CONFIG_APP_WIFI_STA_SSID, sizeof(active_config.wifi_aps[0].ssid));
    strlcpy(active_config.wifi_aps[0].password, CONFIG_APP_WIFI_STA_PASS, sizeof(active_config.wifi_aps[0].password));

    // Seed default calibration parameters
    active_config.voltage_coef = 1.0f;
    active_config.voltage_offset = 0.0f;
    active_config.current_coef = 1.0f;
    active_config.current_offset = 0.0f;

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(CONFIG_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "NVS namespace '%s' opened successfully", CONFIG_NAMESPACE);
        
        // Load UDP IP
        size_t required_size = sizeof(active_config.udp_ip);
        err = nvs_get_str(nvs_handle, "udp_ip", active_config.udp_ip, &required_size);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Loaded udp_ip from NVS: %s", active_config.udp_ip);
        } else if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGI(TAG, "udp_ip key not found in NVS, using fallback: %s", active_config.udp_ip);
        } else {
            ESP_LOGW(TAG, "Error reading udp_ip key (0x%x), using fallback: %s", err, active_config.udp_ip);
        }

        // Load UDP Port
        uint16_t port_val = 0;
        err = nvs_get_u16(nvs_handle, "udp_port", &port_val);
        if (err == ESP_OK) {
            active_config.udp_port = port_val;
            ESP_LOGI(TAG, "Loaded udp_port from NVS: %d", active_config.udp_port);
        } else if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGI(TAG, "udp_port key not found in NVS, using fallback: %d", active_config.udp_port);
        } else {
            ESP_LOGW(TAG, "Error reading udp_port key (0x%x), using fallback: %d", err, active_config.udp_port);
        }

        // Load Wi-Fi AP Count
        uint8_t count_val = 0;
        err = nvs_get_u8(nvs_handle, "wifi_count", &count_val);
        if (err == ESP_OK) {
            active_config.wifi_ap_count = count_val;
            ESP_LOGI(TAG, "Loaded wifi_ap_count from NVS: %d", active_config.wifi_ap_count);

            if (active_config.wifi_ap_count > 0) {
                size_t blob_size = sizeof(active_config.wifi_aps);
                err = nvs_get_blob(nvs_handle, "wifi_aps", active_config.wifi_aps, &blob_size);
                if (err == ESP_OK) {
                    ESP_LOGI(TAG, "Loaded wifi_aps blob from NVS");
                } else {
                    ESP_LOGE(TAG, "Error reading wifi_aps blob from NVS (0x%x)", err);
                }
            }
        } else if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGI(TAG, "wifi_count key not found in NVS, using seeded fallback config");
        } else {
            ESP_LOGW(TAG, "Error reading wifi_count key (0x%x), using seeded fallback", err);
        }

        // Load Voltage Coefficient
        uint32_t vol_coef_u32 = 0;
        err = nvs_get_u32(nvs_handle, "vol_coef", &vol_coef_u32);
        if (err == ESP_OK) {
            memcpy(&active_config.voltage_coef, &vol_coef_u32, sizeof(float));
            ESP_LOGI(TAG, "Loaded voltage_coef from NVS: %.4f", active_config.voltage_coef);
        } else if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGI(TAG, "vol_coef key not found in NVS, using fallback: %.4f", active_config.voltage_coef);
        } else {
            ESP_LOGW(TAG, "Error reading vol_coef key (0x%x), using fallback: %.4f", err, active_config.voltage_coef);
        }

        // Load Voltage Offset
        uint32_t vol_offset_u32 = 0;
        err = nvs_get_u32(nvs_handle, "vol_offset", &vol_offset_u32);
        if (err == ESP_OK) {
            memcpy(&active_config.voltage_offset, &vol_offset_u32, sizeof(float));
            ESP_LOGI(TAG, "Loaded voltage_offset from NVS: %.4f", active_config.voltage_offset);
        } else if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGI(TAG, "vol_offset key not found in NVS, using fallback: %.4f", active_config.voltage_offset);
        } else {
            ESP_LOGW(TAG, "Error reading vol_offset key (0x%x), using fallback: %.4f", err, active_config.voltage_offset);
        }

        // Load Current Coefficient
        uint32_t cur_coef_u32 = 0;
        err = nvs_get_u32(nvs_handle, "cur_coef", &cur_coef_u32);
        if (err == ESP_OK) {
            memcpy(&active_config.current_coef, &cur_coef_u32, sizeof(float));
            ESP_LOGI(TAG, "Loaded current_coef from NVS: %.4f", active_config.current_coef);
        } else if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGI(TAG, "cur_coef key not found in NVS, using fallback: %.4f", active_config.current_coef);
        } else {
            ESP_LOGW(TAG, "Error reading cur_coef key (0x%x), using fallback: %.4f", err, active_config.current_coef);
        }

        // Load Current Offset
        uint32_t cur_offset_u32 = 0;
        err = nvs_get_u32(nvs_handle, "cur_offset", &cur_offset_u32);
        if (err == ESP_OK) {
            memcpy(&active_config.current_offset, &cur_offset_u32, sizeof(float));
            ESP_LOGI(TAG, "Loaded current_offset from NVS: %.4f", active_config.current_offset);
        } else if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGI(TAG, "cur_offset key not found in NVS, using fallback: %.4f", active_config.current_offset);
        } else {
            ESP_LOGW(TAG, "Error reading cur_offset key (0x%x), using fallback: %.4f", err, active_config.current_offset);
        }

        nvs_close(nvs_handle);
    } else {
        ESP_LOGW(TAG, "Could not open NVS namespace (0x%x), using defaults", err);
    }

    initialized = true;
    return ESP_OK;
}

void app_config_get(app_config_t *config)
{
    if (!initialized || config == NULL) {
        return;
    }

    if (xSemaphoreTake(config_mutex, portMAX_DELAY) == pdTRUE) {
        memcpy(config, &active_config, sizeof(app_config_t));
        xSemaphoreGive(config_mutex);
    }
}

esp_err_t app_config_set(const app_config_t *config)
{
    if (!initialized || config == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = ESP_OK;

    if (xSemaphoreTake(config_mutex, portMAX_DELAY) == pdTRUE) {
        // Update active cache
        memcpy(&active_config, config, sizeof(app_config_t));

        // Save to NVS
        nvs_handle_t nvs_handle;
        err = nvs_open(CONFIG_NAMESPACE, NVS_READWRITE, &nvs_handle);
        if (err == ESP_OK) {
            err = nvs_set_str(nvs_handle, "udp_ip", active_config.udp_ip);
            if (err == ESP_OK) {
                err = nvs_set_u16(nvs_handle, "udp_port", active_config.udp_port);
            }
            if (err == ESP_OK) {
                err = nvs_set_u8(nvs_handle, "wifi_count", active_config.wifi_ap_count);
            }
            if (err == ESP_OK) {
                err = nvs_set_blob(nvs_handle, "wifi_aps", active_config.wifi_aps, sizeof(active_config.wifi_aps));
            }
            if (err == ESP_OK) {
                uint32_t vol_coef_u32;
                memcpy(&vol_coef_u32, &active_config.voltage_coef, sizeof(float));
                err = nvs_set_u32(nvs_handle, "vol_coef", vol_coef_u32);
            }
            if (err == ESP_OK) {
                uint32_t vol_offset_u32;
                memcpy(&vol_offset_u32, &active_config.voltage_offset, sizeof(float));
                err = nvs_set_u32(nvs_handle, "vol_offset", vol_offset_u32);
            }
            if (err == ESP_OK) {
                uint32_t cur_coef_u32;
                memcpy(&cur_coef_u32, &active_config.current_coef, sizeof(float));
                err = nvs_set_u32(nvs_handle, "cur_coef", cur_coef_u32);
            }
            if (err == ESP_OK) {
                uint32_t cur_offset_u32;
                memcpy(&cur_offset_u32, &active_config.current_offset, sizeof(float));
                err = nvs_set_u32(nvs_handle, "cur_offset", cur_offset_u32);
            }
            if (err == ESP_OK) {
                err = nvs_commit(nvs_handle);
            }
            nvs_close(nvs_handle);

            if (err == ESP_OK) {
                ESP_LOGI(TAG, "Successfully persisted configuration changes to NVS");
            } else {
                ESP_LOGE(TAG, "Failed to write configuration to NVS: 0x%x", err);
            }
        } else {
            ESP_LOGE(TAG, "Failed to open NVS for writing: 0x%x", err);
        }

        xSemaphoreGive(config_mutex);
    }

    return err;
}

esp_err_t app_config_factory_reset(void)
{
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = ESP_OK;

    if (xSemaphoreTake(config_mutex, portMAX_DELAY) == pdTRUE) {
        nvs_handle_t nvs_handle;
        err = nvs_open(CONFIG_NAMESPACE, NVS_READWRITE, &nvs_handle);
        if (err == ESP_OK) {
            err = nvs_erase_all(nvs_handle);
            if (err == ESP_OK) {
                err = nvs_commit(nvs_handle);
            }
            nvs_close(nvs_handle);
        }
        xSemaphoreGive(config_mutex);
    }
    return err;
}
