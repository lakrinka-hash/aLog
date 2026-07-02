/**
 * @file adc_task.c
 */

#include "freertos/FreeRTOS.h"  // IWYU pragma: keep
#include "freertos/task.h"
#include "freertos/projdefs.h"
#include "esp_log.h"
#include "esp_err.h"
#include "ads1115_drv.h"
#include "system_state.h"
#include "adc_task.h"

void adc_task(void *pvParameters)
{
    adc_task_args_t *args = (adc_task_args_t *)pvParameters;
    if (args == NULL || args->adc == NULL) {
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI("adc_task", "ADC task started...");
    
    ads1115_set_pga(args->adc, CONFIG_APP_ADS1115_PGA);
    ads1115_set_rate(args->adc, CONFIG_APP_ADS1115_RATE);
    ads1115_set_mode(args->adc, ADS1115_MODE_SINGLE_SHOT);

    esp_err_t err1, err2;

    while (1)
    {
        float v1 = 0.0f, v2 = 0.0f;

        // Channel 1 (AIN0)
        ads1115_set_channel(args->adc, 0);
        err1 = ads1115_start_conversion(args->adc);
        if (err1 == ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(CONFIG_APP_ADC_CONVERT_DELAY_MS));
            err1 = ads1115_get_voltage(args->adc, &v1);
        }
        // Channel 2 (AIN1)
        ads1115_set_channel(args->adc, 1);
        err2 = ads1115_start_conversion(args->adc);
        if (err2 == ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(CONFIG_APP_ADC_CONVERT_DELAY_MS));
            err2 = ads1115_get_voltage(args->adc, &v2);
        }
        // ADC telemetry data container update
        sys_state_set_adc(v1, (err1 != ESP_OK), v2, (err2 != ESP_OK));

        vTaskDelay(pdMS_TO_TICKS(CONFIG_APP_ADC_TASK_DELAY_MS));
    }
}