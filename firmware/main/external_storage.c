/**
 * @file external_storage.c
 * @brief Implementation of custom-buffered SD card file operations and rotation
 */

#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h> 
#include <inttypes.h> // for PRIu32 specifier inside fprintf
#include "esp_log.h"
#include "system_time.h"
#include "external_storage.h"

#if __has_include("sdkconfig.h")
#include "sdkconfig.h"
#endif

#ifndef CONFIG_APP_SD_FLUSH_INTERVAL_SEC
#define CONFIG_APP_SD_FLUSH_INTERVAL_SEC  10
#endif

#ifndef CONFIG_APP_LOG_TASK_DELAY_MS
#define CONFIG_APP_LOG_TASK_DELAY_MS      1000
#endif

static const char *TAG = "sd_card";

/**
 * @brief Total file writes (log cycles) to count before forcing an explicit fflush() and VFS fsync().
 */
#define SD_FLUSH_INTERVAL ((CONFIG_APP_SD_FLUSH_INTERVAL_SEC * 1000) / CONFIG_APP_LOG_TASK_DELAY_MS)

esp_err_t ext_storage_init(ext_storage_t *dev, spi_host_device_t host, int cs, const char *mount_point)
{
    if (!dev || !mount_point) return ESP_ERR_INVALID_ARG;

    dev->logfile = NULL;
    dev->timecount = 0;
    dev->current_date[0] = '\0';

    dev->logbuffer = malloc(CONFIG_APP_SD_LOG_BUFFER_SIZE);
    if (dev->logbuffer == NULL) {
        ESP_LOGE(TAG, "No RAM for internal stream caching buffer!");
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "Stream caching buffer %u bytes allocated at %p",
                CONFIG_APP_SD_LOG_BUFFER_SIZE,
                (void*)dev->logbuffer);

    return sd_spi_init(&dev->sdcard, host, cs, mount_point);
}

esp_err_t ext_storage_mount(ext_storage_t *dev)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    if (dev->sdcard.mounted) return ESP_OK;

    ESP_LOGI(TAG, "Calling sd_spi_drv to mount the SD card...");

    if (sd_spi_mount(&dev->sdcard) != ESP_OK) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t ext_storage_unmount(ext_storage_t *dev)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    if (!dev->sdcard.mounted) return ESP_OK;

    if (dev->logfile != NULL) {
        clearerr(dev->logfile);
        fclose(dev->logfile);
        dev->logfile = NULL;
    }
    
    dev->current_date[0] = '\0';
    dev->timecount = 0;
    ESP_LOGI(TAG, "Active log files closed cleanly");

    return sd_spi_unmount(&dev->sdcard);
}

void ext_storage_deinit(ext_storage_t *dev)
{
    if (!dev) return;
    
    ext_storage_unmount(dev);
    
    if (dev->logbuffer != NULL) {
        free(dev->logbuffer);
        dev->logbuffer = NULL;
    }
    ESP_LOGI(TAG, "Stream caching buffer released, structure deinitialized");
}

/**
 * @brief Internal helper to search next available unsynced log sequence index
 */
static bool priv_FindFreeIndex(ext_storage_t *dev, char *out_path, size_t max_len)
{
    struct stat st;
    for (int i = 1; i <= CONFIG_APP_MAX_UNSYNCED_FILES; i++) {
        if (snprintf(out_path, max_len, "%s/log_unsynced_%05d.csv", dev->sdcard.mount_point, i) >= max_len) {
            return false;
        }
        if (stat(out_path, &st) != 0) {
            return true; // Found unallocated file name range
        }
    }
    ESP_LOGE(TAG, "Maximum unindexed unsynced files threshold reached (%d)!", CONFIG_APP_MAX_UNSYNCED_FILES);
    return false;
}

esp_err_t ext_storage_write_line(ext_storage_t *dev, const char *line)
{
    if (!dev || !line) return ESP_ERR_INVALID_ARG;
    if (!dev->sdcard.mounted) return ESP_ERR_INVALID_STATE;

    if (dev->logfile && ferror(dev->logfile)) {
        ESP_LOGE(TAG, "Sticky stream error detected before write");
        ext_storage_unmount(dev);
        return ESP_FAIL;
    }

    char date_str[16] = {0};
    bool time_synced = sys_time_synced();
    
    if (time_synced) {
        sys_time_get_string(date_str, sizeof(date_str), "%Y-%m-%d");
    } else {
        strlcpy(date_str, "unsynced", sizeof(date_str));
    }

    if (dev->logfile == NULL || strcmp(dev->current_date, date_str) != 0)
    {
        if (dev->logfile != NULL) {
            fclose(dev->logfile);
            dev->logfile = NULL;
            ESP_LOGI(TAG, "Log file was closed due to day rotation or disconnect");
        }

        char full_path[64] = {0};
        bool file_exist = false;

        if (time_synced) {
            snprintf(full_path, sizeof(full_path), "%s/log_%s.csv", dev->sdcard.mount_point, date_str);
        } else {
            if (!priv_FindFreeIndex(dev, full_path, sizeof(full_path))) {
                ext_storage_unmount(dev);
                return ESP_ERR_INVALID_STATE;
            }
        }

        struct stat st;
        file_exist = (stat(full_path, &st) == 0);

        dev->logfile = fopen(full_path, "a");
        if (dev->logfile == NULL) {
            ESP_LOGE(TAG, "Failed to open target log file %s", full_path);
            ext_storage_unmount(dev);
            return ESP_FAIL;
        }

        setvbuf(dev->logfile, dev->logbuffer, _IOFBF, CONFIG_APP_SD_LOG_BUFFER_SIZE);
        strlcpy(dev->current_date, date_str, sizeof(dev->current_date));

        if (!file_exist) {
            fprintf(dev->logfile, "Timestamp;Voltage;Voltage_Err;Current;Current_Err;Power;Relay1;Relay2\n");
            if (fflush(dev->logfile) != 0) {
                ext_storage_unmount(dev);
                return ESP_FAIL;
            }
            ESP_LOGI(TAG, "The new log file \"%s\" was created successfully", full_path);
        } else {
            ESP_LOGI(TAG, "The existing log file \"%s\" was opened successfully", full_path);
        }
        dev->timecount = 0;
    }

    uint64_t timestamp = sys_time_get_timestamp_us();
    uint32_t sec = (uint32_t)(timestamp / 1000000ULL);
    uint32_t usec = (uint32_t)(timestamp % 1000000ULL);

    clearerr(dev->logfile);

    int str = fprintf(dev->logfile, "%" PRIu32 ".%06" PRIu32 ";%s\n", sec, usec, line);

    if (str < 0 || ferror(dev->logfile)) {
        ESP_LOGE(TAG, "Write transaction or stream fail detected inside fprintf!");
        ext_storage_unmount(dev);
        return ESP_FAIL;
    }

    dev->timecount++;
    if (dev->timecount >= SD_FLUSH_INTERVAL) 
    {
        if (fflush(dev->logfile) != 0 || ferror(dev->logfile)) {
            ESP_LOGE(TAG, "Buffered fflush failed!");
            ext_storage_unmount(dev);
            return ESP_FAIL;
        }
        int fd = fileno(dev->logfile);
        if (fd >= 0) {
            if (fsync(fd) != 0) {
                ESP_LOGE(TAG, "Physical fsync failed! Hardware layer dead.");
                ext_storage_unmount(dev);
                return ESP_FAIL;
            }
        }
        dev->timecount = 0;
    }
    return ESP_OK;
}
