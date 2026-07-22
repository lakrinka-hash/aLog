/**
 * @file web_server.c
 * @brief Resilient, zero-dependency Espressif HTTP Web Server implementation
 */

#include <inttypes.h>
#include <time.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>

#include "web_server.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "system_state.h"
#include "app_config.h"
#include "wifi_app.h"
#include "esp_chip_info.h"
#include "esp_system.h"
#include "internal_storage.h"
#include "external_storage.h"
#include <dirent.h>
#include <unistd.h>




static const char *TAG = "web_server";
static httpd_handle_t server = NULL;
static ext_storage_t *s_sd_ctx = NULL;

/* Simple fallback diagnostic page in case the SPIFFS image was not flashed or got corrupted */
static const char* INDEX_HTML = 
"<!DOCTYPE html><html><head><title>aLog Error</title>"
"<style>body{font-family:sans-serif;padding:40px;background:#f7fafc;color:#2d3748;line-height:1.6;}.container{max-width:600px;display:block;margin:0 auto;background:white;padding:30px;border-radius:12px;border:1px solid #e2e8f0;}</style></head>"
"<body><div class=\"container\"><h2>SPIFFS Storage Warning</h2>"
"<p>The storage partition is mounted successfully, but <strong>/storage/index.html</strong> was not found on the flash.</p>"
"<p>Make sure you have populated the <code>firmware/storage</code> folder and flashed the SPIFFS partition using: <br>"
"<code>idf.py build flash</code></p></div></body></html>";

static void ensure_default_files(void)
{
    struct stat st;
    if (stat("/storage/index.html", &st) != 0) {
        ESP_LOGW(TAG, "/storage/index.html not found, creating diagnostic page...");
        FILE *f = fopen("/storage/index.html", "w");
        if (f != NULL) {
            fputs(INDEX_HTML, f);
            fclose(f);
            ESP_LOGI(TAG, "Diagnostic fallback page written to SPIFFS successfully");
        } else {
            ESP_LOGE(TAG, "Failed to create fallback index.html");
        }
    }
}

/* Generic GET file handler serving files from /storage */
static esp_err_t file_server_get_handler(httpd_req_t *req)
{
    char filepath[128];
    const char *uri = req->uri;
    
    // If URI is exactly "/", serve index.html
    if (strcmp(uri, "/") == 0) {
        snprintf(filepath, sizeof(filepath), "/storage/index.html");
    } else {
        // Strip query parameters if any (e.g. /index.html?v=1)
        char clean_uri[64];
        strlcpy(clean_uri, uri, sizeof(clean_uri));
        char *quest = strchr(clean_uri, '?');
        if (quest) {
            *quest = '\0';
        }
        snprintf(filepath, sizeof(filepath), "/storage%s", clean_uri);
    }

    // Open file
    FILE *f = fopen(filepath, "r");
    if (f == NULL) {
        // If index.html is missing, check if we can fall back or write default
        if (strcmp(filepath, "/storage/index.html") == 0) {
            ESP_LOGW(TAG, "index.html missing from SPIFFS! Generating default...");
            ensure_default_files();
            f = fopen(filepath, "r");
        }
    }

    if (f == NULL) {
        // If favicon.ico is missing, fallback to embedded tiny representation
        if (strcmp(filepath, "/storage/favicon.ico") == 0) {
            httpd_resp_set_type(req, "image/x-icon");
            static const char favicon_data[] = {
                0x47, 0x49, 0x46, 0x38, 0x39, 0x61, 0x01, 0x00, 0x01, 0x00, 0x80, 0x00,
                0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x21, 0xf9, 0x04, 0x01, 0x00,
                0x00, 0x00, 0x00, 0x2c, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00,
                0x00, 0x02, 0x02, 0x44, 0x01, 0x00, 0x3b
            };
            return httpd_resp_send(req, favicon_data, sizeof(favicon_data));
        }

        // File not found, send 404
        ESP_LOGW(TAG, "File not found: %s", filepath);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File does not exist");
        return ESP_FAIL;
    }

    // Set content type based on extension
    if (strstr(filepath, ".html")) {
        httpd_resp_set_type(req, "text/html");
    } else if (strstr(filepath, ".css")) {
        httpd_resp_set_type(req, "text/css");
    } else if (strstr(filepath, ".js")) {
        httpd_resp_set_type(req, "application/javascript");
    } else if (strstr(filepath, ".png")) {
        httpd_resp_set_type(req, "image/png");
    } else if (strstr(filepath, ".ico")) {
        httpd_resp_set_type(req, "image/x-icon");
    } else if (strstr(filepath, ".json")) {
        httpd_resp_set_type(req, "application/json");
    } else {
        httpd_resp_set_type(req, "text/plain");
    }

    // Read and send file in chunks
    char *chunk_buf = malloc(1024);
    if (!chunk_buf) {
        fclose(f);
        ESP_LOGE(TAG, "No memory to allocate chunk buffer");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return ESP_ERR_NO_MEM;
    }

    size_t read_bytes;
    esp_err_t err = ESP_OK;
    do {
        read_bytes = fread(chunk_buf, 1, 1024, f);
        if (read_bytes > 0) {
            err = httpd_resp_send_chunk(req, chunk_buf, read_bytes);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to send chunk (%s)", esp_err_to_name(err));
                break;
            }
        }
    } while (read_bytes > 0);

    free(chunk_buf);
    fclose(f);

    // Send empty chunk to signal end of response
    httpd_resp_send_chunk(req, NULL, 0);
    return err;
}

/* Endpoint GET /api/data */
static esp_err_t api_data_get_handler(httpd_req_t *req)
{
    char json_buf[512];
    sys_state_adc_t adc;
    sys_state_relays_t relays;
    sys_state_sd_t sd;
    sys_state_time_t time_st;

    sys_state_get_adc(&adc);
    sys_state_get_relays(&relays);
    sys_state_get_sd(&sd);
    sys_state_get_time(&time_st);

    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    char str_time[32];
    strftime(str_time, sizeof(str_time), "%Y-%m-%d %H:%M:%S", &timeinfo);

    uint32_t uptime_s = (uint32_t)(esp_timer_get_time() / 1000000ULL);

    snprintf(json_buf, sizeof(json_buf),
             "{"
             "\"voltage\":%.4f,"
             "\"voltage_err\":%s,"
             "\"current\":%.4f,"
             "\"current_err\":%s,"
             "\"relay1\":%s,"
             "\"relay2\":%s,"
             "\"sd_err\":%s,"
             "\"sd_overflow\":%s,"
             "\"sd_total\":%" PRIu32 ","
             "\"sd_free\":%" PRIu32 ","
             "\"time\":\"%s\","
             "\"time_synced\":%s,"
             "\"time_source\":%d,"
             "\"uptime\":%" PRIu32
             "}",
             adc.voltage, adc.voltage_err ? "true" : "false",
             adc.current, adc.current_err ? "true" : "false",
             relays.relay1 ? "true" : "false",
             relays.relay2 ? "true" : "false",
             sd.err ? "true" : "false",
             sd.overflow ? "true" : "false",
             sd.total_space,
             sd.free_space,
             str_time,
             time_st.synced ? "true" : "false",
             (int)time_st.source,
             uptime_s);

    httpd_resp_set_type(req, "application/json");
    // Prevent client caching so browser requests get actual values every time
    httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    return httpd_resp_send(req, json_buf, HTTPD_RESP_USE_STRLEN);
}

/* Endpoint POST /api/relay?id=1&state=0 */
static esp_err_t api_relay_post_handler(httpd_req_t *req)
{
    char query[64] = {0};
    int id = 0;
    int state = 0;

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char id_str[10] = {0};
        char state_str[10] = {0};
        if (httpd_query_key_value(query, "id", id_str, sizeof(id_str)) == ESP_OK) {
            id = atoi(id_str);
        }
        if (httpd_query_key_value(query, "state", state_str, sizeof(state_str)) == ESP_OK) {
            state = atoi(state_str);
        }

        sys_state_relays_t relays;
        sys_state_get_relays(&relays);

        if (id == 1) {
            relays.relay1 = (state != 0);
            gpio_set_level(CONFIG_APP_RLY1_PIN, relays.relay1 ? 1 : 0);
            ESP_LOGI(TAG, "Web: Relay 1 toggled to %s", relays.relay1 ? "ON" : "OFF");
        } else if (id == 2) {
            relays.relay2 = (state != 0);
            gpio_set_level(CONFIG_APP_RLY2_PIN, relays.relay2 ? 1 : 0);
            ESP_LOGI(TAG, "Web: Relay 2 toggled to %s", relays.relay2 ? "ON" : "OFF");
        }

        sys_state_set_relays(relays.relay1, relays.relay2);
    }

    char response[64];
    snprintf(response, sizeof(response), "{\"status\":\"ok\",\"relay\":%d,\"state\":%d}", id, state);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

/* Endpoint GET /api/settings */
static esp_err_t api_settings_get_handler(httpd_req_t *req)
{
    app_config_t cfg;
    app_config_get(&cfg);

    char *json_buf = malloc(1024);
    if (json_buf == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return ESP_FAIL;
    }

    int len = snprintf(json_buf, 1024,
             "{\"udp_ip\":\"%s\",\"udp_port\":%u,\"voltage_coef\":%.4f,\"voltage_offset\":%.4f,\"current_coef\":%.4f,\"current_offset\":%.4f,\"wifi_aps\":[",
             cfg.udp_ip, cfg.udp_port, cfg.voltage_coef, cfg.voltage_offset, cfg.current_coef, cfg.current_offset);

    for (int i = 0; i < cfg.wifi_ap_count; i++) {
        len += snprintf(json_buf + len, 1024 - len,
                        "%s{\"ssid\":\"%s\"}",
                        (i > 0) ? "," : "",
                        cfg.wifi_aps[i].ssid);
    }

    snprintf(json_buf + len, 1024 - len, "]}");

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    esp_err_t err = httpd_resp_send(req, json_buf, HTTPD_RESP_USE_STRLEN);
    free(json_buf);
    return err;
}

/* Endpoint POST /api/wifi/add */
static esp_err_t api_wifi_add_handler(httpd_req_t *req)
{
    char buf[128] = {0};
    int ret, remaining = req->content_len;

    if (remaining >= sizeof(buf)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content too long");
        return ESP_FAIL;
    }

    int received = 0;
    while (remaining > 0) {
        if ((ret = httpd_req_recv(req, buf + received, remaining)) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            return ESP_FAIL;
        }
        remaining -= ret;
        received += ret;
    }

    char query[128] = {0};
    bool has_query = false;

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        has_query = true;
    } else if (received > 0) {
        strlcpy(query, buf, sizeof(query));
        has_query = true;
    }

    if (has_query) {
        char ssid[32] = {0};
        char pass[64] = {0};

        bool ok_ssid = (httpd_query_key_value(query, "ssid", ssid, sizeof(ssid)) == ESP_OK);
        httpd_query_key_value(query, "password", pass, sizeof(pass));

        if (ok_ssid && strlen(ssid) > 0) {
            app_config_t cfg;
            app_config_get(&cfg);

            int existing_idx = -1;
            for (int i = 0; i < cfg.wifi_ap_count; i++) {
                if (strcmp(cfg.wifi_aps[i].ssid, ssid) == 0) {
                    existing_idx = i;
                    break;
                }
            }

            if (existing_idx != -1) {
                strlcpy(cfg.wifi_aps[existing_idx].password, pass, sizeof(cfg.wifi_aps[existing_idx].password));
                app_config_set(&cfg);
                ESP_LOGI(TAG, "Wi-Fi: Updated password for existing SSID: %s", ssid);
                wifi_app_disconnect();
            } else if (cfg.wifi_ap_count < MAX_WIFI_NETWORKS) {
                int idx = cfg.wifi_ap_count;
                strlcpy(cfg.wifi_aps[idx].ssid, ssid, sizeof(cfg.wifi_aps[idx].ssid));
                strlcpy(cfg.wifi_aps[idx].password, pass, sizeof(cfg.wifi_aps[idx].password));
                cfg.wifi_ap_count++;
                app_config_set(&cfg);
                ESP_LOGI(TAG, "Wi-Fi: Added new SSID: %s", ssid);
                wifi_app_disconnect();
            } else {
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Maximum AP limit reached");
                return ESP_FAIL;
            }
        }
    }

    char response[64];
    snprintf(response, sizeof(response), "{\"status\":\"ok\"}");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

/* Endpoint POST /api/wifi/delete */
static esp_err_t api_wifi_delete_handler(httpd_req_t *req)
{
    char buf[128] = {0};
    int ret, remaining = req->content_len;

    if (remaining >= sizeof(buf)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content too long");
        return ESP_FAIL;
    }

    int received = 0;
    while (remaining > 0) {
        if ((ret = httpd_req_recv(req, buf + received, remaining)) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            return ESP_FAIL;
        }
        remaining -= ret;
        received += ret;
    }

    char query[128] = {0};
    bool has_query = false;

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        has_query = true;
    } else if (received > 0) {
        strlcpy(query, buf, sizeof(query));
        has_query = true;
    }

    if (has_query) {
        char ssid[32] = {0};

        if (httpd_query_key_value(query, "ssid", ssid, sizeof(ssid)) == ESP_OK) {
            app_config_t cfg;
            app_config_get(&cfg);

            int delete_idx = -1;
            for (int i = 0; i < cfg.wifi_ap_count; i++) {
                if (strcmp(cfg.wifi_aps[i].ssid, ssid) == 0) {
                    delete_idx = i;
                    break;
                }
            }

            if (delete_idx != -1) {
                for (int i = delete_idx; i < cfg.wifi_ap_count - 1; i++) {
                    cfg.wifi_aps[i] = cfg.wifi_aps[i + 1];
                }
                memset(&cfg.wifi_aps[cfg.wifi_ap_count - 1], 0, sizeof(wifi_ap_t));
                cfg.wifi_ap_count--;
                app_config_set(&cfg);
                ESP_LOGI(TAG, "Wi-Fi: Deleted SSID: %s", ssid);
                wifi_app_disconnect();
            }
        }
    }

    char response[64];
    snprintf(response, sizeof(response), "{\"status\":\"ok\"}");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

/* Endpoint POST /api/settings */
static esp_err_t api_settings_post_handler(httpd_req_t *req)
{
    char buf[128] = {0};
    int ret, remaining = req->content_len;

    if (remaining >= sizeof(buf)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content too long");
        return ESP_FAIL;
    }

    int received = 0;
    while (remaining > 0) {
        if ((ret = httpd_req_recv(req, buf + received, remaining)) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            return ESP_FAIL;
        }
        remaining -= ret;
        received += ret;
    }

    char query[128] = {0};
    bool has_query = false;

    // First try URL query parameters
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        has_query = true;
    } else if (received > 0) {
        // Fallback to post body
        strlcpy(query, buf, sizeof(query));
        has_query = true;
    }

    if (has_query) {
        app_config_t cfg;
        app_config_get(&cfg);

        char ip_str[32] = {0};
        char port_str[16] = {0};
        char vol_coef_str[32] = {0};
        char vol_offset_str[32] = {0};
        char cur_coef_str[32] = {0};
        char cur_offset_str[32] = {0};
        bool updated = false;

        if (httpd_query_key_value(query, "udp_ip", ip_str, sizeof(ip_str)) == ESP_OK) {
            if (strlen(ip_str) > 0 && strlen(ip_str) < 16) {
                strlcpy(cfg.udp_ip, ip_str, sizeof(cfg.udp_ip));
                updated = true;
            }
        }
        if (httpd_query_key_value(query, "udp_port", port_str, sizeof(port_str)) == ESP_OK) {
            int port_val = atoi(port_str);
            if (port_val > 0 && port_val <= 65535) {
                cfg.udp_port = (uint16_t)port_val;
                updated = true;
            }
        }
        if (httpd_query_key_value(query, "voltage_coef", vol_coef_str, sizeof(vol_coef_str)) == ESP_OK) {
            cfg.voltage_coef = strtof(vol_coef_str, NULL);
            updated = true;
        }
        if (httpd_query_key_value(query, "voltage_offset", vol_offset_str, sizeof(vol_offset_str)) == ESP_OK) {
            cfg.voltage_offset = strtof(vol_offset_str, NULL);
            updated = true;
        }
        if (httpd_query_key_value(query, "current_coef", cur_coef_str, sizeof(cur_coef_str)) == ESP_OK) {
            cfg.current_coef = strtof(cur_coef_str, NULL);
            updated = true;
        }
        if (httpd_query_key_value(query, "current_offset", cur_offset_str, sizeof(cur_offset_str)) == ESP_OK) {
            cfg.current_offset = strtof(cur_offset_str, NULL);
            updated = true;
        }

        if (updated) {
            app_config_set(&cfg);
            ESP_LOGI(TAG, "Settings updated via Web: IP=%s, Port=%d, VolCoef=%.4f, VolOffset=%.4f, CurCoef=%.4f, CurOffset=%.4f", 
                     cfg.udp_ip, cfg.udp_port, cfg.voltage_coef, cfg.voltage_offset, cfg.current_coef, cfg.current_offset);
        }
    }

    char response[64];
    snprintf(response, sizeof(response), "{\"status\":\"ok\"}");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

/* Endpoint GET /api/system/status */
static esp_err_t api_system_status_handler(httpd_req_t *req)
{
    size_t total_spiffs = 0, used_spiffs = 0;
    esp_err_t spiffs_err = int_storage_get_space(&total_spiffs, &used_spiffs);

    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    char json_buf[512];
    snprintf(json_buf, sizeof(json_buf),
             "{"
             "\"free_heap\":%u,"
             "\"min_free_heap\":%u,"
             "\"idf_version\":\"%s\","
             "\"chip_model\":\"ESP32-C3\","
             "\"chip_cores\":%d,"
             "\"chip_revision\":%d,"
             "\"spiffs_total\":%u,"
             "\"spiffs_used\":%u,"
             "\"spiffs_err\":%s"
             "}",
             (unsigned int)esp_get_free_heap_size(),
             (unsigned int)esp_get_minimum_free_heap_size(),
             esp_get_idf_version(),
             (int)chip_info.cores,
             (int)chip_info.revision,
             (unsigned int)total_spiffs,
             (unsigned int)used_spiffs,
             (spiffs_err == ESP_OK) ? "false" : "true"
    );

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    return httpd_resp_send(req, json_buf, HTTPD_RESP_USE_STRLEN);
}

/* Endpoint POST /api/system/reboot */
static esp_err_t api_system_reboot_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Reboot requested via Web Interface. Restarting in 1 second...");
    char response[64];
    snprintf(response, sizeof(response), "{\"status\":\"ok\"}");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
    
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    return ESP_OK;
}

/* Endpoint POST /api/system/factory-reset */
static esp_err_t api_system_factory_reset_handler(httpd_req_t *req)
{
    ESP_LOGW(TAG, "Factory Reset requested via Web Interface. Clearing NVS and restarting in 1 second...");
    esp_err_t err = app_config_factory_reset();
    
    char response[128];
    if (err == ESP_OK) {
        snprintf(response, sizeof(response), "{\"status\":\"ok\"}");
    } else {
        snprintf(response, sizeof(response), "{\"status\":\"error\",\"message\":\"Failed to clear configuration\"}");
    }
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
    
    if (err == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    }
    return ESP_OK;
}

/* Endpoint GET /api/logs */
static esp_err_t api_logs_get_handler(httpd_req_t *req)
{
    ext_storage_t *sd = s_sd_ctx;
    if (!sd || !sd->sdcard.mounted) {
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, "{\"mounted\":false,\"files\":[]}", -1);
    }

    DIR *dir = opendir("/sdcard");
    if (dir == NULL) {
        ESP_LOGE(TAG, "Failed to open /sdcard directory");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, "{\"mounted\":true,\"error\":\"Could not open directory\",\"files\":[]}", -1);
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    httpd_resp_send_chunk(req, "{\"mounted\":true,\"files\":[", -1);

    struct dirent *entry;
    bool first = true;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG || entry->d_type == DT_UNKNOWN) {
            // Match files starting with "log" and ending in ".csv"
            if (strncmp(entry->d_name, "log", 3) == 0 && strstr(entry->d_name, ".csv") != NULL) {
                char full_path[300];
                snprintf(full_path, sizeof(full_path), "/sdcard/%s", entry->d_name);
                struct stat st;
                size_t file_size = 0;
                if (stat(full_path, &st) == 0) {
                    file_size = st.st_size;
                }

                char file_json[350];
                int len = snprintf(file_json, sizeof(file_json), 
                                   "%s{\"name\":\"%s\",\"size\":%u}", 
                                   first ? "" : ",", 
                                   entry->d_name, 
                                   (unsigned int)file_size);
                first = false;
                httpd_resp_send_chunk(req, file_json, len);
            }
        }
    }
    closedir(dir);

    httpd_resp_send_chunk(req, "]}", 2);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

/* Endpoint GET /api/logs/download?file=... */
static esp_err_t api_logs_download_handler(httpd_req_t *req)
{
    char query[128] = {0};
    char filename[64] = {0};

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
        httpd_query_key_value(query, "file", filename, sizeof(filename)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing 'file' parameter");
        return ESP_FAIL;
    }

    // Security check to avoid directory traversal
    if (strstr(filename, "..") != NULL || strchr(filename, '/') != NULL || strchr(filename, '\\') != NULL) {
        httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Invalid file path");
        return ESP_FAIL;
    }

    ext_storage_t *sd = s_sd_ctx;
    if (!sd || !sd->sdcard.mounted) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "SD card not mounted");
        return ESP_FAIL;
    }

    // Flush active log file stream before serving it
    if (sd->logfile != NULL) {
        fflush(sd->logfile);
    }

    char filepath[128];
    snprintf(filepath, sizeof(filepath), "/sdcard/%s", filename);

    FILE *f = fopen(filepath, "r");
    if (f == NULL) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Log file not found");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/csv");
    
    char disposition[128];
    snprintf(disposition, sizeof(disposition), "attachment; filename=\"%s\"", filename);
    httpd_resp_set_hdr(req, "Content-Disposition", disposition);
    httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");

    char *chunk_buf = malloc(1024);
    if (!chunk_buf) {
        fclose(f);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return ESP_ERR_NO_MEM;
    }

    size_t read_bytes;
    esp_err_t err = ESP_OK;
    do {
        read_bytes = fread(chunk_buf, 1, 1024, f);
        if (read_bytes > 0) {
            err = httpd_resp_send_chunk(req, chunk_buf, read_bytes);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to send chunk (%s)", esp_err_to_name(err));
                break;
            }
        }
    } while (read_bytes > 0);

    free(chunk_buf);
    fclose(f);

    httpd_resp_send_chunk(req, NULL, 0);
    return err;
}

/* Endpoint POST /api/logs/delete?file=... or body file=... */
static esp_err_t api_logs_delete_handler(httpd_req_t *req)
{
    char query[128] = {0};
    char filename[64] = {0};
    
    char buf[128] = {0};
    int ret, remaining = req->content_len;

    if (remaining >= sizeof(buf)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content too long");
        return ESP_FAIL;
    }

    int received = 0;
    while (remaining > 0) {
        if ((ret = httpd_req_recv(req, buf + received, remaining)) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            return ESP_FAIL;
        }
        remaining -= ret;
        received += ret;
    }

    bool has_query = false;
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        has_query = true;
    } else if (received > 0) {
        strlcpy(query, buf, sizeof(query));
        has_query = true;
    }

    if (!has_query || httpd_query_key_value(query, "file", filename, sizeof(filename)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing 'file' parameter");
        return ESP_FAIL;
    }

    if (strstr(filename, "..") != NULL || strchr(filename, '/') != NULL || strchr(filename, '\\') != NULL) {
        httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Invalid file path");
        return ESP_FAIL;
    }

    ext_storage_t *sd = s_sd_ctx;
    if (!sd || !sd->sdcard.mounted) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "SD card not mounted");
        return ESP_FAIL;
    }

    char active_path_sync[64] = {0};
    if (sd->current_date[0] != '\0') {
        if (strcmp(sd->current_date, "unsynced") != 0) {
            snprintf(active_path_sync, sizeof(active_path_sync), "log_%s.csv", sd->current_date);
        }
    }

    if (sd->logfile != NULL && strcmp(filename, active_path_sync) == 0) {
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, "{\"status\":\"error\",\"message\":\"Cannot delete the active log file\"}", -1);
    }

    char filepath[128];
    snprintf(filepath, sizeof(filepath), "/sdcard/%s", filename);

    if (unlink(filepath) == 0) {
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, "{\"status\":\"ok\"}", -1);
    } else {
        char response[128];
        snprintf(response, sizeof(response), "{\"status\":\"error\",\"message\":\"Failed to delete file\"}");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, response, -1);
    }
}

/* Endpoint URI registration structures */
static const httpd_uri_t api_data_uri = {
    .uri       = "/api/data",
    .method    = HTTP_GET,
    .handler   = api_data_get_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t api_relay_uri = {
    .uri       = "/api/relay",
    .method    = HTTP_POST,
    .handler   = api_relay_post_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t api_settings_get_uri = {
    .uri       = "/api/settings",
    .method    = HTTP_GET,
    .handler   = api_settings_get_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t api_settings_post_uri = {
    .uri       = "/api/settings",
    .method    = HTTP_POST,
    .handler   = api_settings_post_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t api_wifi_add_uri = {
    .uri       = "/api/wifi/add",
    .method    = HTTP_POST,
    .handler   = api_wifi_add_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t api_wifi_delete_uri = {
    .uri       = "/api/wifi/delete",
    .method    = HTTP_POST,
    .handler   = api_wifi_delete_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t api_system_status_uri = {
    .uri       = "/api/system/status",
    .method    = HTTP_GET,
    .handler   = api_system_status_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t api_system_reboot_uri = {
    .uri       = "/api/system/reboot",
    .method    = HTTP_POST,
    .handler   = api_system_reboot_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t api_system_factory_reset_uri = {
    .uri       = "/api/system/factory-reset",
    .method    = HTTP_POST,
    .handler   = api_system_factory_reset_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t api_logs_get_uri = {
    .uri       = "/api/logs",
    .method    = HTTP_GET,
    .handler   = api_logs_get_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t api_logs_download_uri = {
    .uri       = "/api/logs/download",
    .method    = HTTP_GET,
    .handler   = api_logs_download_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t api_logs_delete_uri = {
    .uri       = "/api/logs/delete",
    .method    = HTTP_POST,
    .handler   = api_logs_delete_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t file_server_uri = {
    .uri       = "/*",
    .method    = HTTP_GET,
    .handler   = file_server_get_handler,
    .user_ctx  = NULL
};

esp_err_t web_server_start(ext_storage_t *sd_ctx)
{
    s_sd_ctx = sd_ctx;
    if (server != NULL) {
        ESP_LOGE(TAG, "Web server is already running");
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    // Allow port 80 (standard HTTP)
    config.server_port = 80;
    // Enable wildcard matching
    config.uri_match_fn = httpd_uri_match_wildcard;
    // Set URI wildcards, stack and priority
    config.ctrl_port = 32768; // avoids colliding with standard ports
    config.max_open_sockets = 4;
    config.lru_purge_enable = true;
    config.max_uri_handlers = 20;

    // Ensure we have the default files written to internal storage
    ensure_default_files();

    ESP_LOGI(TAG, "Starting web server on port %d...", config.server_port);
    esp_err_t err = httpd_start(&server, &config);
    if (err == ESP_OK) {
        // Register API handlers FIRST so they match exactly
        httpd_register_uri_handler(server, &api_data_uri);
        httpd_register_uri_handler(server, &api_relay_uri);
        httpd_register_uri_handler(server, &api_settings_get_uri);
        httpd_register_uri_handler(server, &api_settings_post_uri);
        httpd_register_uri_handler(server, &api_wifi_add_uri);
        httpd_register_uri_handler(server, &api_wifi_delete_uri);
        httpd_register_uri_handler(server, &api_system_status_uri);
        httpd_register_uri_handler(server, &api_system_reboot_uri);
        httpd_register_uri_handler(server, &api_system_factory_reset_uri);
        httpd_register_uri_handler(server, &api_logs_get_uri);
        httpd_register_uri_handler(server, &api_logs_download_uri);
        httpd_register_uri_handler(server, &api_logs_delete_uri);
        // Register file server URI wildcard LAST as a catch-all
        httpd_register_uri_handler(server, &file_server_uri);
        ESP_LOGI(TAG, "Web server endpoints registered successfully");
    } else {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(err));
    }
    return err;
}

void web_server_stop(void)
{
    if (server != NULL) {
        ESP_LOGI(TAG, "Stopping web server...");
        httpd_stop(server);
        server = NULL;
    }
}
