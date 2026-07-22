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
#include <stdint.h>
#include "adc_task.h"
#include "app_config.h"

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
    bool data_ready = false;

    while (1)
    {
        float v1 = 0.0f, v2 = 0.0f;

        // Channel 1 (AIN0)
        ads1115_set_channel(args->adc, 0);
        err1 = ads1115_start_conversion(args->adc);
        if (err1 == ESP_OK) {
            int retries = 15;
            do {
                vTaskDelay(pdMS_TO_TICKS(CONFIG_APP_ADC_CONVERT_DELAY_MS));
                data_ready = false;
                esp_err_t ready_err = ads1115_data_ready(args->adc, &data_ready);
                if (ready_err != ESP_OK) {
                    err1 = ready_err;
                    break;
                }
            } while (!data_ready && --retries > 0);

            if (err1 == ESP_OK && !data_ready) {
                err1 = ESP_ERR_TIMEOUT;
            }
            if (err1 == ESP_OK) {
                err1 = ads1115_get_voltage(args->adc, &v1);
            }
        }

        // Channel 2 (AIN1)
        ads1115_set_channel(args->adc, 1);
        err2 = ads1115_start_conversion(args->adc);
        if (err2 == ESP_OK) {
            int retries = 15;
            do {
                vTaskDelay(pdMS_TO_TICKS(CONFIG_APP_ADC_CONVERT_DELAY_MS));
                data_ready = false;
                esp_err_t ready_err = ads1115_data_ready(args->adc, &data_ready);
                if (ready_err != ESP_OK) {
                    err2 = ready_err;
                    break;
                }
            } while (!data_ready && --retries > 0);

            if (err2 == ESP_OK && !data_ready) {
                err2 = ESP_ERR_TIMEOUT;
            }
            if (err2 == ESP_OK) {
                err2 = ads1115_get_voltage(args->adc, &v2);
            }
        }
        // ADC telemetry data container update
        app_config_t cfg;
        app_config_get(&cfg);

        float calib_v1 = v1;
        float calib_v2 = v2;

        if (err1 == ESP_OK) {
            calib_v1 = v1 * cfg.voltage_coef + cfg.voltage_offset;
        }
        if (err2 == ESP_OK) {
            calib_v2 = v2 * cfg.current_coef + cfg.current_offset;
        }

        sys_state_set_adc(calib_v1, (err1 != ESP_OK), calib_v2, (err2 != ESP_OK));

        vTaskDelay(pdMS_TO_TICKS(CONFIG_APP_ADC_TASK_DELAY_MS));
    }
}