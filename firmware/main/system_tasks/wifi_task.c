/**
 * @file wifi_task.c
 */

#include <stdbool.h>
#include "freertos/FreeRTOS.h"  // IWYU pragma: keep
#include "freertos/task.h"
#include "freertos/projdefs.h"
#include "esp_log.h"
#include "mdns.h"
#include "wifi_app.h"
#include "web_server.h"
#include "system_state.h"
#include "wifi_task.h"

static const char *TAG = "wifi_task";

static bool mdns_initialized = false;

static void start_mdns_service(void)
{
    if (mdns_initialized) return;
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

static void stop_mdns_service(void)
{
    ESP_LOGI(TAG, "Stopping mDNS service...");
    mdns_free();
}

void wifi_task(void *pvParameters)
{
    ESP_LOGI("wifi_task", "Starting Wi-Fi supervisor task...");
    
    // Set STA mode credentials
    if (wifi_app_set_cred(CONFIG_APP_WIFI_STA_SSID, CONFIG_APP_WIFI_STA_PASS) != ESP_OK) {
        ESP_LOGE("wifi_task", "Failed to configure STA credentials");
        vTaskDelete(NULL);
        return;
    }

    // Start Wi-Fi driver interface
    if (wifi_app_start() != ESP_OK) {
        ESP_LOGE("wifi_task", "Failed to start Wi-Fi driver interface!");
        vTaskDelete(NULL);
        return;
    }

    // Set initial state and trigger connection
    sys_state_set_wifi(WIFI_STATE_CONNECTING, "0.0.0.0");
    ESP_LOGI(TAG, "Attempting initial connection to AP: %s...", CONFIG_APP_WIFI_STA_SSID);
    wifi_app_connect();

    wifi_app_state_t last_state = WIFI_STATE_CONNECTING;

    while (1) {
        // Block until notified of a state change (e.g., from the event callback),
        // or check back every 10 seconds as a watchdog check.
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10000));
        
        sys_state_wifi_t wifi_state;
        sys_state_get_wifi(&wifi_state);

        // Transition logic: Start/Stop web server on state change
        if (wifi_state.state == WIFI_STATE_CONNECTED && last_state != WIFI_STATE_CONNECTED) {
            ESP_LOGI(TAG, "Wi-Fi Connected! Starting Web Server...");
            web_server_start();
            start_mdns_service();
        } 
        else if (wifi_state.state != WIFI_STATE_CONNECTED && last_state == WIFI_STATE_CONNECTED) {
            ESP_LOGW(TAG, "Wi-Fi Connection Lost! Stopping Web Server...");
            web_server_stop();
        }
        
        // Reconnection logic: Trigger reconnect on disconnect event
        if (wifi_state.state == WIFI_STATE_DISCONNECTED) {
            ESP_LOGW(TAG, "Retrying in 5 sec connection...");
            sys_state_set_wifi(WIFI_STATE_RECONNECTING, "0.0.0.0");
            vTaskDelay(pdMS_TO_TICKS(5000));
            wifi_app_connect();
        }

        last_state = wifi_state.state;
    }
}