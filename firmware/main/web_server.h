/**
 * @file web_server.h
 * @brief Resilient, zero-dependency Espressif HTTP Web Server for aLog_project.
 *        Serves Web Console completely from internal SPI Flash (NVS/ROM).
 */

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "esp_err.h"
#include "external_storage.h"

/**
 * @brief Initializes GPIO actuators for Relays and starts the web server with the storage context.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t web_server_start(ext_storage_t *sd_ctx);

/**
 * @brief Stops the web server and cleans up resources.
 */
void web_server_stop(void);

#endif // WEB_SERVER_H