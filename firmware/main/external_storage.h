/**
 * @file external_storage.h
 * @brief High-level SD card logging interface with implicit hot-plugging, fatfs caching, and date-based rotation.
 */

#ifndef EXTERNAL_STORAGE_H
#define EXTERNAL_STORAGE_H

#include <stdio.h>          // for FILE*
#include <stdint.h>         // for uint32_t, uint64_t
#include <stdbool.h>        // for bool
#include "esp_err.h"        // for esp_err_t
#include "hal/spi_types.h"  // for spi_host_device_t
#include "sd_spi_drv.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @struct ext_storage_t
 * @brief Master runtime context of the high-level SD card logger.
 */ 
typedef struct 
{ 
    FILE *logfile;           ///< C standard library stream handle to the currently active log file
    char current_date[16];   ///< Cached date string ("YYYY-MM-DD" or "unsynced") used for file rotation detection
    char *logbuffer;         ///< Pointer to the heap-allocated stream buffer passed to setvbuf() to minimize SD bus wear
    uint32_t timecount;      ///< Internal loop counter representing the number of un-flushed writes
    sd_spi_t sdcard;         ///< Embedded low-level SD SPI driver hardware context
} ext_storage_t;

/**
 * @brief Initialize the storage structure and set default values.
 * 
 * @param[out] dev         Pointer to the destination ext_storage_t context to initialize
 * @param[in]  host        The SPI host peripheral index (e.g. SPI2_HOST or SPI3_HOST)
 * @param[in]  cs          GPIO designated as Chip Select (CS) line for the SD card
 * @param[in]  mount_point Virtual VFS mount folder path in ESP-IDF (e.g. "/sdcard")
 * 
 * @return 
 * - ESP_OK                Successful initialization of runtime context and driver hookup
 * - ESP_ERR_INVALID_ARG   Provided device pointer, host, or mount point parameters are invalid or empty
 */ 
esp_err_t ext_storage_init(ext_storage_t *dev, spi_host_device_t host, int cs, const char *mount_point);

/**
 * @brief Mount the SD card physical interface and bind virtual filesystem.
 * 
 * @param[in,out] dev Pointer to the initialized device struct
 * 
 * @return
 * - ESP_OK    SD card mounted successfully and file systems are ready to use
 * - ESP_FAIL  Hardware mount or communication handshake failure (e.g. missing card)
 * - ESP_ERR_NO_MEM Heap allocation for local FATFS write buffer failed
 * - ESP_ERR_INVALID_ARG Received NULL pointer argument
 */
esp_err_t ext_storage_mount(ext_storage_t *dev);

/**
 * @brief Unmount the physical SD card and cleanly close all streams.
 * 
 * @param[in,out] dev Pointer to the active device structure
 * 
 * @return
 * - ESP_OK SD card and VFS streams disconnected cleanly
 * - ESP_ERR_INVALID_ARG Received NULL pointer argument
 */ 
esp_err_t ext_storage_unmount(ext_storage_t *dev);

/**
 * @brief Main high-level interface to append formatted log lines onto the SD card.
 * 
 * @param[in,out] dev Pointer to the active device structure
 * @param[in]     line Semicolon-delimited sensors dataset string to register
 * 
 * @return 
 * - ESP_OK                      Log transaction executed and buffered successfully
 * - ESP_ERR_INVALID_ARG         Provided parameters are NULL pointers
 * - ESP_ERR_INVALID_STATE       SD card physical layer is not currently mounted or active
 * - ESP_FAIL                    Severe streaming or filesystem write exception occurred (forces unmount)
 */
esp_err_t ext_storage_write_line(ext_storage_t *dev, const char *line);

/**
 * @brief Fully deinitialize the logging subsystem and release allocated heap memory.
 * 
 * @param[in,out] dev Pointer to the active device structure
 */
void ext_storage_deinit(ext_storage_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* EXTERNAL_STORAGE_H */
