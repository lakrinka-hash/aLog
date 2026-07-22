/**
 * @file time_task.c
 * @brief Implementation of the system time and RTC synchronization task
 */

#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "time_task.h"
#include "system_state.h"
#include "ds1307_drv.h"

static const char *TAG = "time_task";

void time_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Time task started");
    time_task_args_t *args = (time_task_args_t *)pvParameters;
    const ds1307_t *rtc_dev = NULL;

    if (args != NULL) {
        rtc_dev = args->rtc;
    }

    while (1) {
        // Block indefinitely until notified by the SNTP synchronization callback
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        ESP_LOGI(TAG, "Received SNTP synchronization trigger");

        // Update global system state to indicate SNTP is now the source of truth
        sys_state_set_time(TIME_SRC_SNTP, true);

        // If RTC is attached, write the synchronized POSIX time back to the RTC
        if (rtc_dev != NULL) {
            time_t now;
            time(&now);
            struct tm timeinfo;
            if (localtime_r(&now, &timeinfo) != NULL) {
                ESP_LOGI(TAG, "Writing synchronized time to DS1307 RTC...");
                esp_err_t err = ds1307_set_time(rtc_dev, &timeinfo);
                if (err == ESP_OK) {
                    ESP_LOGI(TAG, "DS1307 RTC updated successfully!");
                } else {
                    ESP_LOGE(TAG, "Failed to update DS1307 RTC: %s", esp_err_to_name(err));
                }
            } else {
                ESP_LOGE(TAG, "Failed to convert time to structure");
            }
        } else {
            ESP_LOGW(TAG, "SNTP synchronized, but no physical RTC is registered to update");
        }
    }
}
