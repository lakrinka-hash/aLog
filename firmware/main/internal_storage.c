/**
 * @file internal_storage.c
 * @brief SPIFFS internal storage mount and space query implementation.
 */

#include <stdbool.h>
#include "esp_log.h"
#include "esp_spiffs.h"
#include "internal_storage.h"

static bool spiffs_mounted = false;

esp_err_t int_storage_mount(void)
{
    if (spiffs_mounted) {
        ESP_LOGI("spiffs", "SPIFFS already mounted");
        return ESP_OK;
    }

    ESP_LOGI("spiffs", "Initializing SPIFFS...");

    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/storage",
        .partition_label = "storage",
        .max_files = 5,
        .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE("spiffs", "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE("spiffs", "Failed to find SPIFFS partition 'storage'");
        } else {
            ESP_LOGE("spiffs", "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ESP_FAIL;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info("storage", &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE("spiffs", "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI("spiffs", "Partition size: %d KB, used: %d KB", (int)(total / 1024), (int)(used / 1024));
    }

    spiffs_mounted = true;
    return ESP_OK;
}

esp_err_t int_storage_unmount(void)
{
    if (!spiffs_mounted) {
        return ESP_OK;
    }

    esp_err_t ret = esp_vfs_spiffs_unregister("storage");
    if (ret == ESP_OK) {
        spiffs_mounted = false;
        ESP_LOGI("spiffs", "SPIFFS unmounted cleanly");
    } else {
        ESP_LOGE("spiffs", "Failed to unregister SPIFFS (%s)", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t int_storage_get_space(size_t *total_bytes, size_t *used_bytes)
{
    if (!spiffs_mounted) {
        return ESP_ERR_INVALID_STATE;
    }

    return esp_spiffs_info("storage", total_bytes, used_bytes);
}
