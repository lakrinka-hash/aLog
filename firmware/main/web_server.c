/**
 * @file web_server.c
 * @brief Resilient, zero-dependency Espressif HTTP Web Server implementation
 */

#include <inttypes.h>
#include <time.h>

#include "web_server.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "system_state.h"




static const char *TAG = "web_server";
static httpd_handle_t server = NULL;

/* Beautiful, modular, lightweight, high-contrast dashboard */
static const char* INDEX_HTML = 
"<!DOCTYPE html>\n"
"<html lang=\"en\">\n"
"<head>\n"
"    <meta charset=\"UTF-8\">\n"
"    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
"    <title>aLog Project Web Console</title>\n"
"    <link rel=\"icon\" href=\"data:image/svg+xml,<svg xmlns=%22http://www.w3.org/2000/svg%22 viewBox=%220 0 100 100%22><text y=%22.9em%22 font-size=%2290%22>📊</text></svg>\">\n"
"    <style>\n"
"        :root {\n"
"            --bg-color: #f7fafc;\n"
"            --card-bg: #ffffff;\n"
"            --text-color: #2d3748;\n"
"            --accent-color: #4a5568;\n"
"            --border-color: #e2e8f0;\n"
"            --success-color: #38a169;\n"
"            --danger-color: #e53e3e;\n"
"        }\n"
"        body {\n"
"            font-family: -apple-system, BlinkMacSystemFont, \"Segoe UI\", Roboto, Helvetica, Arial, sans-serif;\n"
"            background-color: var(--bg-color);\n"
"            color: var(--text-color);\n"
"            margin: 0;\n"
"            padding: 20px;\n"
"        }\n"
"        .container {\n"
"            max-width: 800px;\n"
"            margin: 0 auto;\n"
"        }\n"
"        header {\n"
"            display: flex;\n"
"            justify-content: space-between;\n"
"            align-items: center;\n"
"            border-bottom: 2px solid var(--border-color);\n"
"            padding-bottom: 20px;\n"
"            margin-bottom: 25px;\n"
"        }\n"
"        h1 { margin: 0; font-size: 24px; font-weight: 600; }\n"
"        .status-badge {\n"
"            padding: 6px 12px;\n"
"            border-radius: 20px;\n"
"            font-size: 13px;\n"
"            font-weight: 500;\n"
"            background-color: var(--success-color);\n"
"            color: white;\n"
"        }\n"
"        .grid {\n"
"            display: grid;\n"
"            grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));\n"
"            gap: 20px;\n"
"            margin-bottom: 25px;\n"
"        }\n"
"        .card {\n"
"            background: var(--card-bg);\n"
"            border: 1px solid var(--border-color);\n"
"            border-radius: 12px;\n"
"            padding: 20px;\n"
"            box-shadow: 0 4px 6px -1px rgba(0,0,0,0.05);\n"
"        }\n"
"        .card h3 {\n"
"            margin: 0 0 10px 0;\n"
"            font-size: 14px;\n"
"            text-transform: uppercase;\n"
"            letter-spacing: 0.5px;\n"
"            color: #718096;\n"
"        }\n"
"        .value {\n"
"            font-size: 28px;\n"
"            font-weight: bold;\n"
"            color: #1a202c;\n"
"        }\n"
"        .unit { font-size: 16px; font-weight: normal; color: #718096; margin-left: 4px; }\n"
"        .error { color: var(--danger-color); font-size: 18px; }\n"
"        .controls {\n"
"            display: flex;\n"
"            flex-direction: column;\n"
"            gap: 15px;\n"
"        }\n"
"        .control-row {\n"
"            display: flex;\n"
"            justify-content: space-between;\n"
"            align-items: center;\n"
"        }\n"
"        .switch {\n"
"            position: relative;\n"
"            display: inline-block;\n"
"            width: 50px;\n"
"            height: 26px;\n"
"        }\n"
"        .switch input { opacity: 0; width: 0; height: 0; }\n"
"        .slider {\n"
"            position: absolute;\n"
"            cursor: pointer;\n"
"            top: 0; left: 0; right: 0; bottom: 0;\n"
"            background-color: #cbd5e0;\n"
"            transition: .3s;\n"
"            border-radius: 34px;\n"
"        }\n"
"        .slider:before {\n"
"            position: absolute;\n"
"            content: \"\";\n"
"            height: 18px; width: 18px;\n"
"            left: 4px; bottom: 4px;\n"
"            background-color: white;\n"
"            transition: .3s;\n"
"            border-radius: 50%;\n"
"        }\n"
"        input:checked + .slider { background-color: var(--success-color); }\n"
"        input:checked + .slider:before { transform: translateX(24px); }\n"
"        .footer {\n"
"            margin-top: 40px;\n"
"            text-align: center;\n"
"            font-size: 12px;\n"
"            color: #a0aec0;\n"
"        }\n"
"    </style>\n"
"</head>\n"
"<body>\n"
"    <div class=\"container\">\n"
"        <header>\n"
"            <div>\n"
"                <h1>aLog Telemetry</h1>\n"
"                <div style=\"font-size:13px;color:#718096;margin-top:4px;\" id=\"sys-time\">Syncing time...</div>\n"
"            </div>\n"
"            <span class=\"status-badge\" id=\"wifi-status\">ONLINE</span>\n"
"        </header>\n"
"        \n"
"        <div class=\"grid\">\n"
"            <div class=\"card\">\n"
"                <h3>Voltage</h3>\n"
"                <div class=\"value\" id=\"val-voltage\">--<span class=\"unit\">V</span></div>\n"
"            </div>\n"
"            <div class=\"card\">\n"
"                <h3>Current</h3>\n"
"                <div class=\"value\" id=\"val-current\">--<span class=\"unit\">A</span></div>\n"
"            </div>\n"
"            <div class=\"card\">\n"
"                <h3>Power</h3>\n"
"                <div class=\"value\" id=\"val-power\">--<span class=\"unit\">W</span></div>\n"
"            </div>\n"
"        </div>\n"
"\n"
"        <div class=\"grid\">\n"
"            <div class=\"card\">\n"
"                <h3>SD Card Storage</h3>\n"
"                <div class=\"value\" style=\"font-size:18px; line-height: 1.4;\" id=\"val-sd\">--</div>\n"
"            </div>\n"
"            <div class=\"card\">\n"
"                <h3>Relay Actuators</h3>\n"
"                <div class=\"controls\">\n"
"                    <div class=\"control-row\">\n"
"                        <span>Relay 1 (GPIO 2)</span>\n"
"                        <label class=\"switch\">\n"
"                            <input type=\"checkbox\" id=\"relay1\" onchange=\"toggleRelay(1, this.checked)\">\n"
"                            <span class=\"slider\"></span>\n"
"                        </label>\n"
"                    </div>\n"
"                    <div class=\"control-row\">\n"
"                        <span>Relay 2 (GPIO 3)</span>\n"
"                        <label class=\"switch\">\n"
"                            <input type=\"checkbox\" id=\"relay2\" onchange=\"toggleRelay(2, this.checked)\">\n"
"                            <span class=\"slider\"></span>\n"
"                        </label>\n"
"                    </div>\n"
"                </div>\n"
"            </div>\n"
"        </div>\n"
"\n"
"        <div class=\"footer\">\n"
"            aLog System Device Dashboard &bull; Serving from internal Flash &bull; Uptime: <span id=\"uptime\">0</span>s\n"
"        </div>\n"
"    </div>\n"
"\n"
"    <script>\n"
"        function updateData() {\n"
"            fetch('/api/data')\n"
"                .then(r => r.json())\n"
"                .then(data => {\n"
"                    document.getElementById('sys-time').innerText = 'Internal RTC: ' + data.time;\n"
"                    document.getElementById('uptime').innerText = data.uptime;\n"
"                    document.getElementById('wifi-status').innerText = 'ONLINE';\n"
"                    document.getElementById('wifi-status').style.backgroundColor = '#38a169';\n"
"                    \n"
"                    if (data.voltage_err) {\n"
"                        document.getElementById('val-voltage').innerHTML = '<span class=\"error\">ERROR</span>';\n"
"                    } else {\n"
"                        document.getElementById('val-voltage').innerHTML = data.voltage.toFixed(4) + '<span class=\"unit\">V</span>';\n"
"                    }\n"
"\n"
"                    if (data.current_err) {\n"
"                        document.getElementById('val-current').innerHTML = '<span class=\"error\">ERROR</span>';\n"
"                    } else {\n"
"                        document.getElementById('val-current').innerHTML = data.current.toFixed(4) + '<span class=\"unit\">A</span>';\n"
"                    }\n"
"\n"
"                    if (data.voltage_err || data.current_err) {\n"
"                        document.getElementById('val-power').innerHTML = '<span class=\"error\">ERROR</span>';\n"
"                    } else {\n"
"                        document.getElementById('val-power').innerHTML = (data.voltage * data.current).toFixed(4) + '<span class=\"unit\">W</span>';\n"
"                    }\n"
"\n"
"                    if (data.sd_err) {\n"
"                        document.getElementById('val-sd').innerHTML = '<span class=\"error\">DISCONNECTED</span>';\n"
"                    } else if (data.sd_overflow) {\n"
"                        document.getElementById('val-sd').innerHTML = '<span class=\"error\">CARD FULL</span>';\n"
"                    } else {\n"
"                        document.getElementById('val-sd').innerHTML = \n"
"                            'Free: ' + data.sd_free + ' Mb<br><span style=\"font-size:12px;color:#718096\">Total Capacity: ' + data.sd_total + ' Mb</span>';\n"
"                    }\n"
"\n"
"                    document.getElementById('relay1').checked = data.relay1;\n"
"                    document.getElementById('relay2').checked = data.relay2;\n"
"                })\n"
"                .catch(err => {\n"
"                    console.error(\"Polling error:\", err);\n"
"                    document.getElementById('wifi-status').innerText = 'OFFLINE';\n"
"                    document.getElementById('wifi-status').style.backgroundColor = '#e53e3e';\n"
"                });\n"
"        }\n"
"\n"
"        function toggleRelay(id, state) {\n"
"            fetch('/api/relay?id=' + id + '&state=' + (state ? 1 : 0), { method: 'POST' })\n"
"                .then(r => r.json())\n"
"                .then(data => {\n"
"                    console.log('Relay ' + id + ' set to ' + state);\n"
"                })\n"
"                .catch(err => console.error(\"Toggle error:\", err));\n"
"        }\n"
"\n"
"        setInterval(updateData, 1500);\n"
"        updateData();\n"
"    </script>\n"
"</body>\n"
"</html>";

/* Endpoint GET / */
static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

/* Endpoint GET /favicon.ico */
static esp_err_t favicon_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "image/x-icon");
    // Standard tiny 1x1 transparent GIF representation
    static const char favicon_data[] = {
        0x47, 0x49, 0x46, 0x38, 0x39, 0x61, 0x01, 0x00, 0x01, 0x00, 0x80, 0x00,
        0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x21, 0xf9, 0x04, 0x01, 0x00,
        0x00, 0x00, 0x00, 0x2c, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00,
        0x00, 0x02, 0x02, 0x44, 0x01, 0x00, 0x3b
    };
    return httpd_resp_send(req, favicon_data, sizeof(favicon_data));
}

/* Endpoint GET /api/data */
static esp_err_t api_data_get_handler(httpd_req_t *req)
{
    char json_buf[384];
    sys_state_adc_t adc;
    sys_state_relays_t relays;
    sys_state_sd_t sd;

    sys_state_get_adc(&adc);
    sys_state_get_relays(&relays);
    sys_state_get_sd(&sd);

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

/* Endpoint URI registration structures */
static const httpd_uri_t root_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = root_get_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t favicon_uri = {
    .uri       = "/favicon.ico",
    .method    = HTTP_GET,
    .handler   = favicon_get_handler,
    .user_ctx  = NULL
};

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

esp_err_t web_server_start(void)
{
    if (server != NULL) {
        ESP_LOGE(TAG, "Web server is already running");
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    // Allow port 80 (standard HTTP)
    config.server_port = 80;
    // Set URI wildcards, stack and priority
    config.ctrl_port = 32768; // avoids colliding with standard ports
    config.max_open_sockets = 4;
    config.lru_purge_enable = true;

    ESP_LOGI(TAG, "Starting web server on port %d...", config.server_port);
    esp_err_t err = httpd_start(&server, &config);
    if (err == ESP_OK) {
        httpd_register_uri_handler(server, &root_uri);
        httpd_register_uri_handler(server, &favicon_uri);
        httpd_register_uri_handler(server, &api_data_uri);
        httpd_register_uri_handler(server, &api_relay_uri);
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
