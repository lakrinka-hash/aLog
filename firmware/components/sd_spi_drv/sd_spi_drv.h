/**
 * @file sd_spi_drv.h
 * @brief SD card SPI driver component for ESP-IDF using VFS FAT.
 */

#ifndef SD_SPI_DRV_H
#define SD_SPI_DRV_H

#include <stdbool.h>  // for bool
#include <stdint.h>   // for uint32_t

#include "esp_err.h"
#include "hal/spi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @struct sd_spi_t
 * @brief Structure for the SD card SPI driver context.
 */
typedef struct
{
    spi_host_device_t host;  ///< Shared SPI host instance (e.g., SPI2_HOST).
    void *card_handle;       ///< Opaque pointer to internal storage context (sdmmc_card_t).
    char mount_point[32];    ///< Virtual filesystem mount path (e.g., "/sdcard").
    int cs;                  ///< GPIO pin number used for Chip Select (CS).
    bool mounted;            ///< Device mount status flag (true = successfully mounted).
} sd_spi_t;

/**
 * @brief Initializes the SD card SPI driver context structure.
 * @param[in,out] dev         Pointer to the SD card device structure.
 * @param[in]     host        SPI host ID (e.g., SPI2_HOST).
 * @param[in]     cs          GPIO pin number for Chip Select (CS).
 * @param[in]     mount_point Absolute VFS mount path string (e.g., "/sdcard").
 * @return
 * - ESP_OK                On success.
 * - ESP_ERR_INVALID_ARG   If `dev` or `mount_point` is NULL.
 */
esp_err_t sd_spi_init(sd_spi_t *dev, spi_host_device_t host, int cs, const char *mount_point);

/**
 * @brief Mounts the SD card to the specified mount point.
 * @param[in,out] dev Pointer to the SD card device structure.
 * @return
 * - ESP_OK                 On success.
 * - ESP_ERR_INVALID_ARG    If `dev` is NULL.
 * - ESP_FAIL / ESP_ERR_... On FATFS mounting errors.
 */
esp_err_t sd_spi_mount(sd_spi_t *dev);

/**
 * @brief Unmounts the SD card from the specified mount point.
 * @param[in,out] dev Pointer to the SD card device structure.
 * @return
 * - ESP_OK                 On success.
 * - ESP_ERR_INVALID_ARG    If `dev` is NULL.
 * - ESP_ERR_INVALID_STATE  If the card is not mounted.
 * - ESP_FAIL / ESP_ERR_... On VFS unmounting errors.
 */
esp_err_t sd_spi_unmount(sd_spi_t *dev);

/**
 * @brief Gets the total and free space of the mounted SD card.
 * @param[in]  dev      Pointer to the SD card device structure.
 * @param[out] total_mb Pointer to store the total capacity in Megabytes (MB).
 * @param[out] free_mb  Pointer to store the free space in Megabytes (MB).
 * @return
 * - ESP_OK                 On success.
 * - ESP_ERR_INVALID_ARG    If `dev`, `total_mb`, or `free_mb` is NULL.
 * - ESP_ERR_INVALID_STATE  If the card is not mounted.
 * - ESP_FAIL / ESP_ERR_... On FATFS information retrieval errors.
 */
esp_err_t sd_spi_get_space(sd_spi_t *dev, uint32_t *total_mb, uint32_t *free_mb);

/**
 * @brief Logs information about the mounted SD card.
 * @param[in] dev Pointer to the SD card device structure.
 * @return
 * - ESP_OK                 On success.
 * - ESP_ERR_INVALID_ARG    If `dev` is NULL.
 * - ESP_FAIL / ESP_ERR_... On errors during data reading.
 */
esp_err_t sd_spi_info(sd_spi_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* SD_SPI_DRV_H */