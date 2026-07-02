/**
 * @file udp_task.h
 * @brief 
 */

#ifndef UDP_TASK_H
#define UDP_TASK_H

#if __has_include("sdkconfig.h")
#include "sdkconfig.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CONFIG_APP_UDP_TARGET_IP
#define CONFIG_APP_UDP_TARGET_IP      "10.42.0.1"
#endif

#ifndef CONFIG_APP_UDP_TARGET_PORT
#define CONFIG_APP_UDP_TARGET_PORT    5000
#endif

#ifndef CONFIG_APP_UDP_TASK_DELAY_MS
#define CONFIG_APP_UDP_TASK_DELAY_MS  1000
#endif

/**
 * @brief 
 * @param pvParameters 
 */
void udp_task(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif /* UDP_TASK_H */