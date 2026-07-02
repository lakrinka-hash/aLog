/**
 * @file log_task.h
 * @brief 
 */

#ifndef LOG_TASK_H
#define LOG_TASK_H

#include "system_storage.h"

#if __has_include("sdkconfig.h")
#include "sdkconfig.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CONFIG_APP_LOG_TASK_DELAY_MS
#define CONFIG_APP_LOG_TASK_DELAY_MS     1000
#endif

/**
 * @struct log_task_args_t
 * @brief Arguments passed to the log_task on creation
 */
typedef struct {
    sys_storage_t *storage;  ///< 
} log_task_args_t;

/**
 * @brief 
 * @param pvParameters 
 */
void log_task(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif /* LOG_TASK_H */