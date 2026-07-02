/**
 * @file wifi_app.h
 * @brief Decoupled, modular Wi-Fi Station (STA) component API.
 *        The calling code (e.g. main.c) is responsible for the state machine,
 *        monitoring, and reconnection logic.
 */

#ifndef WIFI_APP_H
#define WIFI_APP_H

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>

typedef enum {
    WIFI_STATE_DISCONNECTED,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_RECONNECTING
} wifi_app_state_t;

typedef void (*wifi_app_state_cb_t)(wifi_app_state_t new_state);

/**
 * @brief Initialize the Wi-Fi TCP/IP stack, default event loop, network interface, and driver.
 *        This function allocated resources but DOES NOT start the Wi-Fi driver interface.
 * @return esp_err_t ESP_OK on success, or appropriate error code.
 */
esp_err_t wifi_app_init(void);

/**
 * @brief Register a callback to be called on Wi-Fi state changes.
 * @param cb The callback function.
 */
void wifi_app_register_state_callback(wifi_app_state_cb_t cb);

/**
 * @brief Configure Wi-Fi Station (STA) credentials.
 *        Can be called before starting, or on-the-fly.
 * @param[in] ssid     SSID of target AP (must not be NULL)
 * @param[in] password Password of target AP (must not be NULL, at least 8 chars)
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t wifi_app_set_cred(const char *ssid, const char *password);

/**
 * @brief Set Wi-Fi mode to Station (STA) and start the Wi-Fi driver interface.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t wifi_app_start(void);

/**
 * @brief Stop the Wi-Fi driver interface.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t wifi_app_stop(void);

/**
 * @brief Hand off a connection request to the Wi-Fi driver.
 *        Should only be called after wifi_app_start() has succeeded.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t wifi_app_connect(void);

/**
 * @brief Disconnect from the currently connected AP.
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t wifi_app_disconnect(void);

/**
 * @brief Retrieve the current IP address string.
 * @param[out] ip_buffer Buffer to copy the IP string into (at least 16 bytes recommended)
 * @param[in]  max_len   Size of the buffer
 * @return esp_err_t ESP_OK on success, or ESP_ERR_INVALID_STATE if not connected.
 */
esp_err_t wifi_app_get_ip(char *ip_buffer, size_t max_len);

#endif // WIFI_APP_H
