/**
 * @file system_time.c
 * @brief Implementation of system time utilities using POSIX time.h
 */

#include <time.h>      // for time, tzset, strftime, struct tm, time_t
#include <sys/time.h>  // for gettimeofday, struct timeval
#include <stdlib.h>    // for setenv

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_sntp.h"
#include "esp_netif_sntp.h"
#include "esp_log.h"
#include "system_time.h"
#include "ds1307_drv.h"
#include "system_state.h"
#include "tasks_list.h"

static const char *TAG = "sys_time";

#define TIMESTAMP_2020_THRESHOLD 1577836800LL  // Unix timestamp for 2020-01-01 00:00:00 UTC

static const ds1307_t *s_rtc_dev = NULL;

static void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Time synchronization event: tv_sec = %ld", (long)tv->tv_sec);
    if (system_task.time_task_handle != NULL) {
        ESP_LOGI(TAG, "Triggering background time_task RTC update...");
        xTaskNotifyGive(system_task.time_task_handle);
    }
}

void sys_time_init(void)
{
    setenv("TZ", "EET-2EEST,M3.5.0/3,M10.5.0/4", 1);
    tzset();
    ESP_LOGI(TAG, "POSIX timezone initialized to Ukraine time (EET/EEST)");
    
    ESP_LOGI(TAG, "Initializing SNTP client...");

    esp_sntp_config_t sntp_cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(
        3,
        ESP_SNTP_SERVER_LIST("pool.ntp.org",
                             "time.google.com",
                             "uk.pool.ntp.org")
    );

    sntp_cfg.sync_cb = time_sync_notification_cb;

    esp_err_t err = esp_netif_sntp_init(&sntp_cfg);
    if (err == ESP_OK) {
        esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb);
        ESP_LOGI(TAG, "SNTP client initialized successfully");
    } else {
        ESP_LOGE(TAG, "Failed to initialize SNTP client: %s", esp_err_to_name(err));
    }
}

void sys_time_get_string(char *buffer, size_t max_len, const char *format)
{
    if (!buffer || max_len == 0) return;

    buffer[0] = '\0';

    time_t now;
    struct tm timeinfo;

    time(&now);
    if (localtime_r(&now, &timeinfo) == NULL) return;

    if (!format) format = "%Y-%m-%d %H:%M:%S";
    strftime(buffer, max_len, format, &timeinfo);
}

uint64_t sys_time_get_timestamp_us(void)
{
    struct timeval tv;
    if (gettimeofday(&tv, NULL) != 0) return 0;
    return ((uint64_t)tv.tv_sec * 1000000ULL) + (uint64_t)tv.tv_usec;
}

bool sys_time_synced(void)
{
    time_t now;
    time(&now);
    return (now > TIMESTAMP_2020_THRESHOLD);
}

void sys_time_register_rtc(const void *rtc_dev)
{
    s_rtc_dev = (const ds1307_t *)rtc_dev;
    if (!s_rtc_dev) return;

    // Check if the DS1307 is halted, and start it if so
    bool halted = false;
    if (ds1307_is_halted(s_rtc_dev, &halted) == ESP_OK && halted) {
        ESP_LOGW(TAG, "DS1307 RTC is halted. Starting oscillator...");
        ds1307_start_oscillator(s_rtc_dev);
    }

    // Check if system time is not currently synchronized (i.e., year < 2020)
    if (!sys_time_synced()) {
        struct tm rtc_tm;
        if (ds1307_get_time(s_rtc_dev, &rtc_tm) == ESP_OK) {
            // Validate that the RTC year is sane (> 2020, which in tm_year is >= 120)
            if (rtc_tm.tm_year >= 120) {
                time_t t = mktime(&rtc_tm);
                if (t != (time_t)-1) {
                    struct timeval tv = { .tv_sec = t, .tv_usec = 0 };
                    settimeofday(&tv, NULL);
                    char buf[32];
                    sys_time_get_string(buf, sizeof(buf), NULL);
                    ESP_LOGI(TAG, "System clock set from DS1307 RTC to: %s", buf);
                    sys_state_set_time(TIME_SRC_RTC, true);
                } else {
                    sys_state_set_time(TIME_SRC_UNSYNC, false);
                }
            } else {
                ESP_LOGW(TAG, "DS1307 has invalid year (%d), skipping initial sync", rtc_tm.tm_year + 1900);
                sys_state_set_time(TIME_SRC_UNSYNC, false);
            }
        } else {
            sys_state_set_time(TIME_SRC_UNSYNC, false);
        }
    } else {
        ESP_LOGI(TAG, "System clock is already synced, keeping current time");
    }
}