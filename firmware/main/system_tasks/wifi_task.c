/**
 * @file wifi_task.c
 * @brief Event-driven Wi-Fi supervisor task using system state notifications
 */

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/projdefs.h"
#include "esp_log.h"
#include "mdns.h"
#include "wifi_app.h"
#include "web_server.h"
#include "system_state.h"
#include "wifi_task.h"
#include "app_config.h"
#include "external_storage.h"

static const char *TAG = "wifi_task";

static bool mdns_initialized = false;

static void start_mdns_service(void)
{
    if (mdns_initialized) {
        ESP_LOGI(TAG, "mDNS service is already initialized, skipping setup.");
        return;
    }

    ESP_LOGI(TAG, "Initializing mDNS service...");
    esp_err_t err = mdns_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mDNS init failed: %s", esp_err_to_name(err));
        return;
    }

    err = mdns_hostname_set("alog");
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mDNS hostname set failed: %s", esp_err_to_name(err));
        return;
    }
    
    err = mdns_instance_name_set("aLog Datalogger");
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mDNS instance name set failed: %s", esp_err_to_name(err));
    }

    // Register standard HTTP service
    err = mdns_service_add("aLog Web Server", "_http", "_tcp", 80, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mDNS service add failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "mDNS service successfully registered. Access at: http://alog.local/");
        mdns_initialized = true;
    }
}

void wifi_task(void *pvParameters)
{
    ext_storage_t *sd_ctx = (ext_storage_t *)pvParameters;
    ESP_LOGI(TAG, "Starting Wi-Fi supervisor task (Multi-AP mode)...");
    
    // Start Wi-Fi driver interface
    if (wifi_app_start() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start Wi-Fi driver interface!");
        vTaskDelete(NULL);
        return;
    }

    app_config_t cfg;
    app_config_get(&cfg);

    int current_ap_idx = 0;
    int try_count = 0;
    wifi_app_state_t last_state = WIFI_STATE_DISCONNECTED;
    uint32_t conn_start_ticks = xTaskGetTickCount();
    bool awaiting_conn = false;

    // Trigger initial connection
    if (cfg.wifi_ap_count > 0) {
        wifi_ap_t *ap = &cfg.wifi_aps[current_ap_idx];
        ESP_LOGI(TAG, "Attempting initial connection to AP[%d]: %s...", current_ap_idx, ap->ssid);
        wifi_app_set_cred(ap->ssid, ap->password);
        sys_state_set_wifi(WIFI_STATE_CONNECTING, "0.0.0.0");
        wifi_app_connect();
        awaiting_conn = true;
        conn_start_ticks = xTaskGetTickCount();
    } else {
        ESP_LOGW(TAG, "No Wi-Fi APs configured in settings!");
        sys_state_set_wifi(WIFI_STATE_DISCONNECTED, "0.0.0.0");
    }

    while (1) {
        // Block until notified of a state change, or check back every 1 second during active connection attempts
        uint32_t wait_ms = awaiting_conn ? 1000 : 10000;
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(wait_ms));

        sys_state_wifi_t wifi_state;
        sys_state_get_wifi(&wifi_state);

        // Transition logic: Start/Stop web server and mDNS on state change
        if (wifi_state.state == WIFI_STATE_CONNECTED && last_state != WIFI_STATE_CONNECTED) {
            ESP_LOGI(TAG, "Wi-Fi Connected! Starting Web Server and mDNS...");
            web_server_start(sd_ctx);
            start_mdns_service();
            awaiting_conn = false;
            try_count = 0;
        } 
        else if (wifi_state.state != WIFI_STATE_CONNECTED && last_state == WIFI_STATE_CONNECTED) {
            ESP_LOGW(TAG, "Wi-Fi Connection Lost! Stopping Web Server...");
            web_server_stop();
            
            // Reload settings in case the list was modified while we were connected
            app_config_get(&cfg);
            awaiting_conn = false;
        }

        // Connection/Reconnection state machine
        if (wifi_state.state != WIFI_STATE_CONNECTED) {
            if (cfg.wifi_ap_count == 0) {
                // Periodically check if any AP was added
                app_config_get(&cfg);
                if (cfg.wifi_ap_count > 0) {
                    current_ap_idx = 0;
                    try_count = 0;
                    wifi_ap_t *ap = &cfg.wifi_aps[current_ap_idx];
                    ESP_LOGI(TAG, "New APs detected! Trying AP[%d]: %s...", current_ap_idx, ap->ssid);
                    wifi_app_set_cred(ap->ssid, ap->password);
                    sys_state_set_wifi(WIFI_STATE_CONNECTING, "0.0.0.0");
                    wifi_app_connect();
                    awaiting_conn = true;
                    conn_start_ticks = xTaskGetTickCount();
                } else {
                    vTaskDelay(pdMS_TO_TICKS(5000));
                }
            } 
            else if (!awaiting_conn) {
                // Start a connection attempt on current AP
                wifi_ap_t *ap = &cfg.wifi_aps[current_ap_idx];
                ESP_LOGI(TAG, "Retrying/Switching connection. Trying AP[%d]: %s...", current_ap_idx, ap->ssid);
                wifi_app_set_cred(ap->ssid, ap->password);
                sys_state_set_wifi(WIFI_STATE_CONNECTING, "0.0.0.0");
                wifi_app_connect();
                awaiting_conn = true;
                conn_start_ticks = xTaskGetTickCount();
            } 
            else {
                // We are awaiting connection, check if we timed out (12 seconds) or if we got disconnected
                uint32_t elapsed_ms = pdTICKS_TO_MS(xTaskGetTickCount() - conn_start_ticks);
                bool failed = false;

                if (wifi_state.state == WIFI_STATE_DISCONNECTED) {
                    ESP_LOGW(TAG, "AP[%d]: %s failed to connect (Disconnected)", current_ap_idx, cfg.wifi_aps[current_ap_idx].ssid);
                    failed = true;
                } else if (elapsed_ms > 12000) {
                    ESP_LOGW(TAG, "AP[%d]: %s connection attempt timed out", current_ap_idx, cfg.wifi_aps[current_ap_idx].ssid);
                    failed = true;
                    // Force disconnection so driver state is clean
                    wifi_app_disconnect();
                }

                if (failed) {
                    awaiting_conn = false;
                    try_count++;
                    
                    // Switch to next AP in the list
                    current_ap_idx = (current_ap_idx + 1) % cfg.wifi_ap_count;

                    // If we've cycled through all APs, delay for a bit before trying again
                    if (try_count >= cfg.wifi_ap_count) {
                        ESP_LOGW(TAG, "All configured APs failed. Waiting 10 seconds before retrying whole list...");
                        sys_state_set_wifi(WIFI_STATE_DISCONNECTED, "0.0.0.0");
                        vTaskDelay(pdMS_TO_TICKS(10000));
                        try_count = 0;
                        // Reload configuration in case it changed during the delay
                        app_config_get(&cfg);
                    } else {
                        // Small delay before trying next AP
                        vTaskDelay(pdMS_TO_TICKS(1000));
                    }
                }
            }
        }

        last_state = wifi_state.state;
    }
}
