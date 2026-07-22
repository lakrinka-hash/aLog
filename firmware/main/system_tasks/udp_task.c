/**
 * @file udp_task.c
 */

#include <stdint.h>             // for uint32_t
#include <stdio.h>              // for sprintf, snprintf
#include <inttypes.h>           // for PRIu32

#include "freertos/FreeRTOS.h"  // IWYU pragma: keep
#include "freertos/task.h"
#include "freertos/projdefs.h"
#include "esp_log.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include "system_state.h"
#include "system_time.h"
#include "wifi_app.h"
#include "app_config.h"
#include "udp_task.h"

void udp_task(void *pvParameters)
{
    ESP_LOGI("log_task", "LOG task started...");

    sys_state_relays_t relays;  // local copy of the relays state
    sys_state_adc_t adc_data;   // local copy of the ADC state
    sys_state_wifi_t wifi_state; // local copy of the Wi-Fi state

    while (1) {
        sys_state_get_wifi(&wifi_state);
        if (wifi_state.state == WIFI_STATE_CONNECTED)
        {
            sys_state_get_adc(&adc_data);
            sys_state_get_relays(&relays);
            float power = 0.0f;
            if (!adc_data.voltage_err && !adc_data.current_err) {
                power = adc_data.voltage * adc_data.current;
            }
            uint64_t timestamp = sys_time_get_timestamp_us();
            uint32_t sec = (uint32_t)(timestamp / 1000000ULL);
            uint32_t usec = (uint32_t)(timestamp % 1000000ULL);

            char line[140];
            int len = snprintf(line, sizeof(line), "%" PRIu32 ".%06" PRIu32 ";%.4f;%d;%.4f;%d;%.4f;%d;%d\n",
                    sec, usec,
                    adc_data.voltage, adc_data.voltage_err ? 1 : 0,
                    adc_data.current, adc_data.current_err ? 1 : 0,
                    power, relays.relay1 ? 1 : 0, relays.relay2 ? 1 : 0);
            
            static int sock = -1;
            if (sock < 0) {
                sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
                if (sock < 0) {
                    ESP_LOGE("udp_log", "Failed to create UDP socket: errno %d", errno);
                    return;
                }
            }

            app_config_t app_cfg;
            app_config_get(&app_cfg);

            struct sockaddr_in dest_addr;
            dest_addr.sin_addr.s_addr = inet_addr(app_cfg.udp_ip);
            dest_addr.sin_family = AF_INET;
            dest_addr.sin_port = htons(app_cfg.udp_port);

            int err = sendto(sock, line, len, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));

            if (err < 0) {
                ESP_LOGE("udp_log", "Failed to send UDP packet: errno %d. Resetting socket.", errno);
                close(sock);
                sock = -1;
            } else {
                ESP_LOGD("udp_log", "Sent UDP packet: %s", line);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(CONFIG_APP_UDP_TASK_DELAY_MS));
    }
}