/**
 * @file time_task.h
 * @brief Header file for the system time and RTC synchronization task
 */

#ifndef TIME_TASK_H
#define TIME_TASK_H

#if __has_include("sdkconfig.h")
#include "sdkconfig.h"
#endif

#include "ds1307_drv.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @struct time_task_args_t
 * @brief Arguments passed to the time_task on creation
 */
typedef struct {
    const ds1307_t *rtc;  ///< Pointer to the DS1307 driver context structure
} time_task_args_t;

/**
 * @brief FreeRTOS task function that manages system time sync and updates DS1307 RTC
 * @param pvParameters Pointer to a time_task_args_t structure containing task parameters
 */
void time_task(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif /* TIME_TASK_H */
