/**
 * @file lcd_task.c
 * @brief Event-driven OLED display rendering task
 */

#include <stdio.h>              // for sprintf, snprintf
#include <stdint.h>             // for uint32_t

#include "freertos/FreeRTOS.h"  // IWYU pragma: keep
#include "freertos/task.h"
#include "freertos/projdefs.h"
#include "esp_log.h"
#include "system_state.h"
#include "lcd_task.h"

static const char *TAG = "lcd_task";

#define SSD1306_PAGE    (CONFIG_APP_SSD1306_HEIGHT / 8)
#define SSD1306_SIZE    (CONFIG_APP_SSD1306_WIDTH * SSD1306_PAGE)

void lcd_task(void *pvParameters)
{
    ESP_LOGI("lcd_task", "LCD task started...");
    lcd_task_args_t *args = (lcd_task_args_t *)pvParameters;
    if (args == NULL || args->lcd == NULL || args->gfx == NULL) {
        ESP_LOGE(TAG, "Invalid arguments passed to lcd_task");
        vTaskDelete(NULL);
        return;
    }

    ssd1306_clear(args->lcd);
    ssd1306_set_mode(args->lcd, SSD1306_MODE_HORIZONTAL);
    ssd1306_set_window(args->lcd, 0, 0, CONFIG_APP_SSD1306_WIDTH-1, SSD1306_PAGE-1);

    sys_state_relays_t relay_state;
    sys_state_adc_t adc_state;
    sys_state_sd_t sd_state;
    sys_state_wifi_t wifi_state;

    char str[24];

    while (1)
    {
        // Block until notified by system_state setters, or wake up every 1s (heartbeat)
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));

        sys_state_get_sd(&sd_state);
        sys_state_get_adc(&adc_state);
        sys_state_get_relays(&relay_state);
        sys_state_get_wifi(&wifi_state);
        gfx_clear(args->gfx, BLACK);

        // 1 string
        if (!adc_state.voltage_err) {
            snprintf(str, sizeof(str), "U=%.4f R1:%s",
                    adc_state.voltage,
                    relay_state.relay1 ? "ON" : "OFF");
        } else {
            snprintf(str, sizeof(str), "U:ERROR R1:%s",
                    relay_state.relay1 ? "ON" : "OFF");
        }
        gfx_draw_string(args->gfx, 0, 0, str, WHITE);

        // 2 string
        if (!adc_state.current_err) {
            snprintf(str, sizeof(str), "A=%.4f R2:%s",
                    adc_state.current,
                    relay_state.relay2 ? "ON" : "OFF");
        } else {
            snprintf(str, sizeof(str), "A:ERROR R2:%s",
                    relay_state.relay2 ? "ON" : "OFF");
        }
        gfx_draw_string(args->gfx, 0, 8, str, WHITE);

        // 3 string
        if (sd_state.overflow) {
            snprintf(str, sizeof(str), "SD:FULL");
        } else {
            snprintf(str, sizeof(str), "SD:%s %" PRIu32 " Mb",
            sd_state.err ? "ERR" : "OK",
            (!sd_state.err) ? sd_state.free_space : (uint32_t)0);
        }
        gfx_draw_string(args->gfx, 0, 16, str, WHITE);

        // 4 string
        switch (wifi_state.state) {
            case WIFI_STATE_DISCONNECTED:
                snprintf(str, sizeof(str), "WIFI: OFFLINE");
                break;
            case WIFI_STATE_CONNECTING:
                snprintf(str, sizeof(str), "WIFI: CONNECTING");
                break;
            case WIFI_STATE_CONNECTED:
                snprintf(str, sizeof(str), "%s", wifi_state.ip_addr);
                break;
            case WIFI_STATE_RECONNECTING:
                snprintf(str, sizeof(str), "WIFI: WAITING");
                break;
            default:
                snprintf(str, sizeof(str), "WIFI: UNKNOWN");
                break;
        }
        gfx_draw_string(args->gfx, 0, 24, str, WHITE);

        ssd1306_write_stream(args->lcd, args->gfx->buffer, SSD1306_SIZE);  // redraw LCD

        vTaskDelay(pdMS_TO_TICKS(CONFIG_APP_LCD_TASK_DELAY_MS));
    }
}