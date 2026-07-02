/**
 * @file pwr_task.h
 * @brief Header file for the power-loss response task
 */

#ifndef PWR_TASK_H
#define PWR_TASK_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief FreeRTOS task function that blocks waiting for power failure interrupt signals,
 *        then gracefully powers down non-essential components and secures logging
 * @param pvParameters Unused task parameters
 */
void pwr_task(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif /* PWR_TASK_H */