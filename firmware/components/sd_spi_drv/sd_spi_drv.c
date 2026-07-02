
/**
 * @file sd_spi_drv.c
 * @brief
 */

#include <string.h>             // for memset, strncpy
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "sd_protocol_types.h"
#include "sd_spi_drv.h"

static const char *TAG = "sd_spi_drv";

esp_err_t sd_spi_init(sd_spi_t *dev, spi_host_device_t host, int cs, const char *mount_point)
{
    if (!dev|| !mount_point) return ESP_ERR_INVALID_ARG;

    dev->host = host;
    dev->cs = cs;
    dev->card_handle = NULL;
    dev->mounted = false;
    strlcpy(dev->mount_point, mount_point, sizeof(dev->mount_point));
    
    return ESP_OK;
}

esp_err_t sd_spi_mount(sd_spi_t *dev)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    if (dev->mounted) {
        ESP_LOGI(TAG, "SD Card (CS=%d) at \"%s\" already mounted",
                dev->cs,
                dev->mount_point);
        return ESP_OK;
    }

    sdmmc_host_t host_config = SDSPI_HOST_DEFAULT();
    host_config.slot = dev->host;

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = dev->cs;
    slot_config.host_id = dev->host;

    esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_card_t *card = NULL;

    esp_err_t ret = esp_vfs_fat_sdspi_mount(dev->mount_point, &host_config, &slot_config, &mount_config, &card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SD Card (CS=%d) on SPI host (ID=%d) at \"%s\". err: %s",
                dev->cs,
                dev->host,
                dev->mount_point,
                esp_err_to_name(ret));
        return ret;
    }

    dev->card_handle = card;
    dev->mounted = true;

    ESP_LOGI(TAG, "SD Card (handle=%p, CS=%d) on SPI host (ID=%d) mounted to \"%s\"",
            dev->card_handle,
            dev->cs,
            dev->host,
            dev->mount_point);
    
    return ESP_OK;
}

esp_err_t sd_spi_unmount(sd_spi_t *dev)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    if (!dev->mounted || !dev->card_handle) return ESP_ERR_INVALID_STATE;

    esp_err_t ret = esp_vfs_fat_sdcard_unmount(dev->mount_point, dev->card_handle);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "SD Card (CS=%d) at \"%s\" cleanly unmounted and closed",
            dev->cs,
            dev->mount_point);
    } else {
        ESP_LOGW(TAG, "Unmount status for \"%s\" (handle=%p): %s (Resources will be cleared anyway)",
            dev->mount_point,
            (void*)dev->card_handle,
            esp_err_to_name(ret));
    }
    
    dev->card_handle = NULL;
    dev->mounted = false;
    return ESP_OK;
}

esp_err_t sd_spi_get_space(sd_spi_t *dev, uint32_t *total_mb, uint32_t *free_mb)
{
    if(!dev) return ESP_ERR_INVALID_ARG;
    if (!dev->card_handle || !dev->mounted) return ESP_ERR_INVALID_STATE;

    uint64_t total_bytes = 0, free_bytes = 0;
    esp_err_t ret = esp_vfs_fat_info(dev->mount_point, &total_bytes, &free_bytes);
    if (ret != ESP_OK) return ret;

    *free_mb = (uint32_t)(free_bytes / (1024ULL * 1024ULL));
    *total_mb = (uint32_t)(total_bytes / (1024ULL * 1024ULL));
    return ESP_OK;
}

esp_err_t sd_spi_info(sd_spi_t *dev)
{
    if(!dev) return ESP_ERR_INVALID_ARG;

    uint32_t total_mb = 0, free_mb = 0;
    esp_err_t ret = sd_spi_get_space(dev, &total_mb, &free_mb);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read FATFS info for \"%s\" (handle=%p). err: %s",
                dev->mount_point,
                (void*)dev->card_handle,
                esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "SD Card \"%s\" (handle=%p) free/total: %u / %u Mb",
            dev->mount_point,
            (void*)dev->card_handle,
            free_mb,
            total_mb);
    return ret;
}