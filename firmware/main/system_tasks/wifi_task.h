/**
 * @file wifi_task.h
 * @brief 
 */

#ifndef WIFI_TASK_H
#define WIFI_TASK_H

#if __has_include("sdkconfig.h")
#include "sdkconfig.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CONFIG_APP_WIFI_STA_SSID
#define CONFIG_APP_WIFI_STA_SSID       "hp840g3"
#endif

#ifndef CONFIG_APP_WIFI_STA_PASS
#define CONFIG_APP_WIFI_STA_PASS       "rst579599"
#endif

#ifndef CONFIG_APP_WIFI_TASK_DELAY_MS
#define CONFIG_APP_WIFI_TASK_DELAY_MS  10000
#endif

/**
 * @brief 
 * @param pvParameters 
 */
void wifi_task(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_TASK_H */