/**
 * @file adc_task.h
 * @brief Header file for the ADC telemetry measurement task
 */

#ifndef ADC_TASK_H
#define ADC_TASK_H

#if __has_include("sdkconfig.h")
#include "sdkconfig.h"
#endif

#include "ads1115_drv.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CONFIG_APP_ADC_TASK_DELAY_MS
#define CONFIG_APP_ADC_TASK_DELAY_MS     500
#endif

#ifndef CONFIG_APP_ADC_CONVERT_DELAY_MS
#define CONFIG_APP_ADC_CONVERT_DELAY_MS  10
#endif

/**
 * @struct adc_task_args_t
 * @brief Arguments passed to the adc_task on creation
 */
typedef struct {
    ads1115_t *adc;  ///< Pointer to the ADS1115 driver context structure
} adc_task_args_t;

/**
 * @brief FreeRTOS task function that monitors and reads voltage levels from the ADS1115
 * @param pvParameters Pointer to an adc_task_args_t structure containing task parameters
 */
void adc_task(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif /* ADC_TASK_H */