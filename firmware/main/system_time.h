/**
 * @file system_time.h
 * @brief Thread-safe time management module
 */

#ifndef SYSTEM_TIME_H
#define SYSTEM_TIME_H

#include <stdint.h>    // for uint64_t
#include <stddef.h>    // for size_t
#include <stdbool.h>   // for bool

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the device time module and configure timezone.
 * @note Must be called during system initialization in app_main.
 */
void sys_time_init(void);

/**
 * @brief Get current formatted time string from system RTC.
 *
 * @param[out] buffer   Destination char buffer to store the string.
 * @param[in]  max_len  Maximum buffer size (including null-terminator).
 * @param[in]  format   strftime-compatible format string (e.g., "%Y-%m-%d").
 * If NULL, default format "%Y-%m-%d %H:%M:%S" is used.
 */
void sys_time_get_string(char *buffer, size_t max_len, const char *format);

/**
 * @brief Get current UNIX timestamp in microseconds.
 * @return uint64_t timestamp
 */
uint64_t sys_time_get_timestamp_us(void);

/**
 * @brief Check if the system time has been synchronized via NTP/external source.
 * @return true if time is valid (year > 2020), false if running on default 1970 epoch.
 */
bool sys_time_synced(void);

#ifdef __cplusplus
}
#endif

#endif /* SYSTEM_TIME_H */