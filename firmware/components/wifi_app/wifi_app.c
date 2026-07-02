/**
 * @file wifi_app.c
 * @brief Decoupled, modular Wi-Fi Station (STA) component implementation.
 */

#include "wifi_app.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"  // IWYU pragma: keep
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "wifi_app";

/* Internal synchronization and state */
static SemaphoreHandle_t state_mutex = NULL;
static esp_netif_t *sta_netif = NULL;
static bool configured = false;
static bool started = false;
static bool connected = false;
static char ip_addr[16] = "0.0.0.0";

static wifi_app_state_cb_t state_callback = NULL;
void wifi_app_register_state_callback(wifi_app_state_cb_t cb) {
    state_callback = cb;
}

static void notify_state_change(wifi_app_state_t new_state) {
    if (state_callback != NULL) {
        state_callback(new_state);
    }
}

/* Event handler handles event updates only without automatic transitions */
static void wifi_app_event_handler(void *arg, esp_event_base_t event_base,
                                   int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "Wi-Fi Interface started");
                notify_state_change(WIFI_STATE_CONNECTING);
                break;
            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "Wi-Fi Station connected to AP. Awaiting IP...");
                notify_state_change(WIFI_STATE_CONNECTING);
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGI(TAG, "Wi-Fi Station disconnected");
                notify_state_change(WIFI_STATE_DISCONNECTED);
                if (state_mutex != NULL) {
                    xSemaphoreTake(state_mutex, portMAX_DELAY);
                    connected = false;
                    strcpy(ip_addr, "0.0.0.0");
                    xSemaphoreGive(state_mutex);
                }
                break;
            default:
                break;
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            if (state_mutex != NULL) {
                xSemaphoreTake(state_mutex, portMAX_DELAY);
                connected = true;
                esp_ip4addr_ntoa(&event->ip_info.ip, ip_addr, sizeof(ip_addr));
                ESP_LOGI(TAG, "Obtained Station IP: %s", ip_addr);
                xSemaphoreGive(state_mutex);
            }
            notify_state_change(WIFI_STATE_CONNECTED);
        }
    }
}

esp_err_t wifi_app_init(void)
{
    if (state_mutex == NULL) {
        state_mutex = xSemaphoreCreateMutex();
        if (state_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create state mutex!");
            return ESP_ERR_NO_MEM;
        }
    }

    /* Initialize TCP/IP and standard system levels if not already running elsewhere */
    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_netif_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_event_loop_create_default failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Create default Wi-Fi Station interface */
    if (sta_netif == NULL) {
        sta_netif = esp_netif_create_default_wifi_sta();
        if (sta_netif == NULL) {
            ESP_LOGE(TAG, "Failed to create default wifi sta interface!");
            return ESP_FAIL;
        }
    }

    /* Init default Wi-Fi config */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Register events */
    ret = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                              &wifi_app_event_handler, NULL, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WIFI_EVENT handler: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                              &wifi_app_event_handler, NULL, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register IP_EVENT_STA_GOT_IP handler: %s", esp_err_to_name(ret));
        return ret;
    }

    xSemaphoreTake(state_mutex, portMAX_DELAY);
    configured = false;
    started = false;
    connected = false;
    strcpy(ip_addr, "0.0.0.0");
    xSemaphoreGive(state_mutex);

    ESP_LOGI(TAG, "Wi-Fi Station stack initialized successfully");
    return ESP_OK;
}

esp_err_t wifi_app_set_cred(const char *ssid, const char *password)
{
    if (ssid == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    wifi_config_t sta_config;
    memset(&sta_config, 0, sizeof(wifi_config_t));
    strncpy((char *)sta_config.sta.ssid, ssid, sizeof(sta_config.sta.ssid) - 1);
    if (password != NULL) {
        strncpy((char *)sta_config.sta.password, password, sizeof(sta_config.sta.password) - 1);
    }

    sta_config.sta.threshold.authmode = password && strlen(password) ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    
    esp_err_t ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_mode failure: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_wifi_set_config(WIFI_IF_STA, &sta_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_config failure: %s", esp_err_to_name(ret));
        return ret;
    }

    if (state_mutex != NULL) {
        xSemaphoreTake(state_mutex, portMAX_DELAY);
        configured = true;
        xSemaphoreGive(state_mutex);
    }

    ESP_LOGI(TAG, "Target SSID configured for Wi-Fi Station: %s", ssid);
    return ESP_OK;
}

esp_err_t wifi_app_start(void)
{
    esp_err_t ret = ESP_OK;
    
    if (state_mutex != NULL) {
        xSemaphoreTake(state_mutex, portMAX_DELAY);
        if (started) {
            xSemaphoreGive(state_mutex);
            ESP_LOGW(TAG, "Wi-Fi is already started.");
            return ESP_OK;
        }
        xSemaphoreGive(state_mutex);
    }

    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_start failed: %s", esp_err_to_name(ret));
        return ret;
    }

    if (state_mutex != NULL) {
        xSemaphoreTake(state_mutex, portMAX_DELAY);
        started = true;
        xSemaphoreGive(state_mutex);
    }

    return ESP_OK;
}

esp_err_t wifi_app_stop(void)
{
    esp_err_t ret = ESP_OK;

    if (state_mutex != NULL) {
        xSemaphoreTake(state_mutex, portMAX_DELAY);
        if (!started) {
            xSemaphoreGive(state_mutex);
            ESP_LOGW(TAG, "Wi-Fi is already stopped.");
            return ESP_OK;
        }
        xSemaphoreGive(state_mutex);
    }

    ret = esp_wifi_stop();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_stop failed: %s", esp_err_to_name(ret));
        return ret;
    }

    if (state_mutex != NULL) {
        xSemaphoreTake(state_mutex, portMAX_DELAY);
        started = false;
        connected = false;
        strcpy(ip_addr, "0.0.0.0");
        xSemaphoreGive(state_mutex);
    }

    return ESP_OK;
}

esp_err_t wifi_app_connect(void)
{
    esp_err_t ret = ESP_OK;

    if (state_mutex != NULL) {
        xSemaphoreTake(state_mutex, portMAX_DELAY);
        if (!started) {
            xSemaphoreGive(state_mutex);
            ESP_LOGE(TAG, "Cannot connect: Wi-Fi stack is not started");
            return ESP_ERR_INVALID_STATE;
        }
        xSemaphoreGive(state_mutex);
    }

    ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_connect failed: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

esp_err_t wifi_app_disconnect(void)
{
    esp_err_t ret = ESP_OK;

    if (state_mutex != NULL) {
        xSemaphoreTake(state_mutex, portMAX_DELAY);
        if (!started) {
            xSemaphoreGive(state_mutex);
            ESP_LOGW(TAG, "Wi-Fi stack is not started");
            return ESP_ERR_INVALID_STATE;
        }
        xSemaphoreGive(state_mutex);
    }

    ret = esp_wifi_disconnect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_disconnect failed: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

esp_err_t wifi_app_get_ip(char *ip_buffer, size_t max_len)
{
    if (ip_buffer == NULL || max_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = ESP_ERR_INVALID_STATE;
    if (state_mutex != NULL) {
        xSemaphoreTake(state_mutex, portMAX_DELAY);
        if (connected) {
            strncpy(ip_buffer, ip_addr, max_len - 1);
            ip_buffer[max_len - 1] = '\0';
            ret = ESP_OK;
        } else {
            strncpy(ip_buffer, "0.0.0.0", max_len - 1);
            ip_buffer[max_len - 1] = '\0';
        }
        xSemaphoreGive(state_mutex);
    }
    return ret;
}
