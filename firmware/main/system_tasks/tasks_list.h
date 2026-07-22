/**
 * @file tasks_list.h
 * @brief Global registry of system FreeRTOS task handles
 */

#ifndef TASKS_LIST_H
#define TASKS_LIST_H

#include "freertos/FreeRTOS.h"  // IWYU pragma: keep
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @typedef system_tasks_t
 * @brief Container holding FreeRTOS task handles for all core tasks in the system
 */
typedef struct
{
    TaskHandle_t adc_task_handle;   ///< Handle for the ADC monitoring task
    TaskHandle_t lcd_task_handle;   ///< Handle for the LCD display task
    TaskHandle_t log_task_handle;   ///< Handle for the SD card logging task
    TaskHandle_t pwr_task_handle;   ///< Handle for the power loss monitoring task
    TaskHandle_t udp_task_handle;   ///< 
    TaskHandle_t wifi_task_handle;  ///< Handle for the Wi-Fi management task
    TaskHandle_t time_task_handle;  ///< Handle for the time management task

} system_task_t;

/**
 * @brief Global instance of system_task_t containing active task descriptors
 */
extern system_task_t system_task;

#ifdef __cplusplus
}
#endif

#endif  /* TASKS_LIST_H */