/**
 * @file pwr_task.c
 */

#include "freertos/FreeRTOS.h"  // IWYU pragma: keep
#include "freertos/task.h"
#include "freertos/projdefs.h"
#include "esp_log.h"
#include "wifi_app.h"
#include "web_server.h"
#include "tasks_list.h"
#include "pwr_task.h"

void pwr_task(void *pvParameters)
{
    ESP_LOGI("pwr_task", "PWR task started...");
    
    while (1)
    {
        // Wait forever block for an interrupt wake-up signal
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        ESP_LOGE("pwr_task", "POWER LOSS DETECTED ON GPIO %d", CONFIG_APP_PWR_CTRL_PIN);

        // Stop Wi-Fi component and web server to minimize power consumption
        web_server_stop();
        wifi_app_stop();

        // Freezing Wi-Fi, LCD and ADC tasks
        if (system_task.wifi_task_handle) {
            vTaskSuspend(system_task.wifi_task_handle);
            ESP_LOGW("pwr_task", "Wi-Fi task suspended.");
        }
        if (system_task.lcd_task_handle) {
            vTaskSuspend(system_task.lcd_task_handle);
            ESP_LOGW("pwr_task", "LCD task suspended.");
        }
        if (system_task.adc_task_handle) {
            vTaskSuspend(system_task.adc_task_handle);
            ESP_LOGW("pwr_task", "ADC task suspended.");
        }

        // Set the max priority for a log task
        if (system_task.log_task_handle) {
            ESP_LOGW("pwr_task", "Boosting Log task...");
            vTaskPrioritySet(system_task.log_task_handle, configMAX_PRIORITIES - 1);
            xTaskNotifyGive(system_task.log_task_handle);
        } else {
            ESP_LOGE("pwr_task", "Log task is missing");
        }

        // Put master system to permanent passive low-power halt loop
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}