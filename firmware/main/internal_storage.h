/**
 * @file internal_storage.h
 * @brief SPIFFS internal storage partition mount and space query interface.
 */

#ifndef INTERNAL_STORAGE_H
#define INTERNAL_STORAGE_H

#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize and mount the internal SPIFFS flash storage partition.
 * 
 * @details Mounts the 960K 'storage' partition to "/storage" using the SPIFFS filesystem.
 * If mounting fails (e.g. unformatted flash), it will automatically format it.
 * 
 * @return 
 * - ESP_OK    SPIFFS mounted successfully and is ready for reading/writing.
 * - ESP_FAIL  Failed to initialize or mount the SPIFFS partition.
 */
esp_err_t int_storage_mount(void);

/**
 * @brief Unmount the internal SPIFFS flash storage partition.
 * 
 * @return
 * - ESP_OK    SPIFFS unmounted successfully.
 */
esp_err_t int_storage_unmount(void);

/**
 * @brief Get space details for the internal SPIFFS flash storage.
 * 
 * @param[out] total_bytes  Total size of the partition in bytes.
 * @param[out] used_bytes   Used space in bytes.
 * 
 * @return
 * - ESP_OK                 Successfully retrieved space statistics.
 * - ESP_ERR_INVALID_STATE  SPIFFS is not mounted.
 */
esp_err_t int_storage_get_space(size_t *total_bytes, size_t *used_bytes);

#ifdef __cplusplus
}
#endif

#endif /* INTERNAL_STORAGE_H */
