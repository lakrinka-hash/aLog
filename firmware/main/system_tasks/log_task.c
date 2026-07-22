/**
 * @file log_task.c
 */

#include <stdint.h>             // for uint16_t
#include <stdio.h>              // for snprintf

#include "freertos/FreeRTOS.h"  // IWYU pragma: keep
#include "freertos/task.h"
#include "freertos/projdefs.h"
#include "esp_log.h"
#include "sd_spi_drv.h"
#include "system_state.h"
#include "log_task.h"

typedef enum {
    STORAGE_STATE_CONNECT,
    STORAGE_STATE_CHECK,
    STORAGE_STATE_OK,
    STORAGE_STATE_FAIL,
    STORAGE_STATE_FULL
} storage_state_t;

void log_task(void *pvParameters)
{
    ESP_LOGI("log_task", "LOG task started...");
    log_task_args_t *args = (log_task_args_t *)pvParameters;
   
    char line[96];
    storage_state_t storage_state = STORAGE_STATE_CONNECT;
    sys_state_relays_t relays;  // local copy of the relays state
    sys_state_adc_t adc_data;   // local copy of the ADC state
    esp_err_t ret;
    uint8_t reconect_count = 0;
    uint16_t space_check_count = 0;
    
    while (1)
    {
        sys_state_get_adc(&adc_data);
        sys_state_get_relays(&relays);
        float power = 0.0f;
        if (!adc_data.voltage_err && !adc_data.current_err) {
            power = adc_data.voltage * adc_data.current;
        }
        snprintf(line, sizeof(line), "%.4f,%d,%.4f,%d,%.4f,%d,%d",
                adc_data.voltage, adc_data.voltage_err ? 1 : 0,
                adc_data.current, adc_data.current_err ? 1 : 0,
                power, relays.relay1 ? 1 : 0, relays.relay2 ? 1 : 0);
        
        switch (storage_state)
        {
            case STORAGE_STATE_CONNECT:
            {
                ret = ext_storage_mount(args->storage);
                if (ret != ESP_OK) {
                    sys_state_set_sd(true, false, 0, 0);
                    ext_storage_unmount(args->storage);
                    storage_state = STORAGE_STATE_FAIL;
                    break;
                }
                space_check_count = 60;  // Force immediate space check after mount
                storage_state = STORAGE_STATE_CHECK;
            }
            //break;
            [[fallthrough]];  // Force logging after reconnection

            case STORAGE_STATE_CHECK:
            {
                if (space_check_count >= 60) {
                    uint32_t total_mb, free_mb;
                    ret = sd_spi_get_space(&(args->storage->sdcard), &total_mb, &free_mb);
                    if (ret != ESP_OK) {
                        sys_state_set_sd(true, false, 0, 0);
                        ext_storage_unmount(args->storage);
                        storage_state = STORAGE_STATE_FAIL;
                        break;
                    }
                    // Guaranteed 20% free space left on the card
                    if ((free_mb * 5) < total_mb) {
                        sys_state_set_sd(false, true, total_mb, free_mb);
                        storage_state = STORAGE_STATE_FULL;
                        break;
                    }
                    sys_state_set_sd(false, false, total_mb, free_mb);
                    space_check_count = 0;
                } else {
                    space_check_count++;
                }
                storage_state = STORAGE_STATE_OK;
            }
            //break;
            [[fallthrough]];  // Force logging after checking free space

            case STORAGE_STATE_OK:
            {
                ret = ext_storage_write_line(args->storage, line);
                if (ret != ESP_OK) {
                    ESP_LOGE("log_task", "Unable to send string to stream buffer");
                    ext_storage_unmount(args->storage);
                    storage_state = STORAGE_STATE_FAIL;
                    break;
                }
                storage_state = STORAGE_STATE_CHECK;
            }
            break;

            case STORAGE_STATE_FAIL:
            {
                if (reconect_count >= 10) {
                    reconect_count = 0;
                    storage_state = STORAGE_STATE_CONNECT;
                    break;
                }
                reconect_count++;
            }
            break;

            case STORAGE_STATE_FULL:
            {
                uint32_t total_mff = 0, free_mff = 0;
                ret = sd_spi_get_space(&(args->storage->sdcard), &total_mff, &free_mff);
                if (ret != ESP_OK) {
                    sys_state_set_sd(true, false, 0, 0);
                    ext_storage_unmount(args->storage);
                    storage_state = STORAGE_STATE_FAIL;
                    break;
                }
                if ((free_mff * 5) >= total_mff) {
                    sys_state_set_sd(false, false, total_mff, free_mff);
                    storage_state = STORAGE_STATE_OK;
                    space_check_count = 60; // Force immediate check if card is no longer full
                }
            }
            break;

            default: break;
        }

        // Wait for next cycle, or wake up instantly on emergency interrupt trigger
        if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(CONFIG_APP_LOG_TASK_DELAY_MS)) == pdTRUE)
        {
            ESP_LOGW("log_task", "Emergency shutdown notification received in logTask!");
            
            // Record one final emergency log entry with the last available sensor readings
            sys_state_get_adc(&adc_data);
            sys_state_get_relays(&relays);
            float power_val = 0.0f;
            if (!adc_data.voltage_err && !adc_data.current_err) {
                power_val = adc_data.voltage * adc_data.current;
            }
            snprintf(line, sizeof(line), "%.4f;%d;%.4f;%d;%.4f;%d;%d",
                    adc_data.voltage, adc_data.voltage_err ? 1 : 0,
                    adc_data.current, adc_data.current_err ? 1 : 0,
                    power_val, relays.relay1 ? 1 : 0, relays.relay2 ? 1 : 0);

            if (storage_state == STORAGE_STATE_OK || storage_state == STORAGE_STATE_CHECK) {
                ext_storage_write_line(args->storage, line);
            }

            if (args->storage->logfile != NULL) {
                ext_storage_unmount(args->storage);
                ESP_LOGI("log_task", "Data saved successfully");
            }
            ESP_LOGW("log_task", "Terminating logTask");
            vTaskDelete(NULL);
        }
    }
}