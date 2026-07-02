/**
 * @file system_state.c
 * @brief Thread-safe implementation of the global system state repository with notification broadcasts
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"  // IWYU pragma: keep
#include "freertos/semphr.h"
#include "system_state.h"
#include "tasks_list.h"

typedef struct {
    sys_state_adc_t    adc;
    sys_state_relays_t relays;
    sys_state_sd_t     sd;
    sys_state_wifi_t   wifi; 
} system_state_t;

static system_state_t system_state = {0};
static SemaphoreHandle_t store_mutex = NULL;

static void broadcast_state_change(void)
{
    // Notify the LCD task to update the display immediately when any state changes
    if (system_task.lcd_task_handle != NULL) {
        xTaskNotifyGive(system_task.lcd_task_handle);
    }
}


void sys_state_init(void)
{
    store_mutex = xSemaphoreCreateMutex();
    configASSERT(store_mutex != NULL);

    system_state.adc.voltage_err = true;
    system_state.adc.current_err = true;

    system_state.sd.err = true;
    system_state.sd.overflow = false;
    system_state.sd.total_space = 0;
    system_state.sd.free_space = 0;

    system_state.wifi.state = WIFI_STATE_DISCONNECTED;
    strcpy(system_state.wifi.ip_addr, "0.0.0.0");
}

/************************ SETTERS (for data providers) ************************/

void sys_state_set_adc(float voltage, bool v_err, float current, bool c_err)
{
    if (store_mutex && xSemaphoreTake(store_mutex, portMAX_DELAY) == pdTRUE) {
        system_state.adc.voltage = voltage;
        system_state.adc.voltage_err = v_err;
        system_state.adc.current = current;
        system_state.adc.current_err = c_err;
        xSemaphoreGive(store_mutex);
        broadcast_state_change();
    }
}

void sys_state_set_sd(bool err, bool overflow, uint32_t total_space, uint32_t free_space)
{
    if (store_mutex && xSemaphoreTake(store_mutex, portMAX_DELAY) == pdTRUE) {
        system_state.sd.err = err;
        system_state.sd.overflow = overflow;
        system_state.sd.total_space = total_space;
        system_state.sd.free_space = free_space;
        xSemaphoreGive(store_mutex);
        broadcast_state_change();
    }
}

void sys_state_set_relays(bool r1, bool r2)
{
    if (store_mutex && xSemaphoreTake(store_mutex, portMAX_DELAY) == pdTRUE) {
        system_state.relays.relay1 = r1;
        system_state.relays.relay2 = r2;
        xSemaphoreGive(store_mutex);
        broadcast_state_change();
    }
}

void sys_state_set_wifi(wifi_app_state_t state, const char *ip_addr)
{
    if (store_mutex && xSemaphoreTake(store_mutex, portMAX_DELAY) == pdTRUE) {
        system_state.wifi.state = state;
        if (ip_addr) {
            strncpy(system_state.wifi.ip_addr, ip_addr, sizeof(system_state.wifi.ip_addr) - 1);
            system_state.wifi.ip_addr[sizeof(system_state.wifi.ip_addr) - 1] = '\0';
        } else {
            strcpy(system_state.wifi.ip_addr, "0.0.0.0");
        }
        xSemaphoreGive(store_mutex);
        
        // Notify both LCD and Wi-Fi tasks on a Wi-Fi state change
        broadcast_state_change();
        if (system_task.wifi_task_handle != NULL) {
            xTaskNotifyGive(system_task.wifi_task_handle);
        }
    }
}

/**************************  GETTERS (for consumers) **************************/

void sys_state_get_adc(sys_state_adc_t *dst)
{
    if (dst && store_mutex && xSemaphoreTake(store_mutex, portMAX_DELAY) == pdTRUE) {
        *dst = system_state.adc;
        xSemaphoreGive(store_mutex);
    }
}

void sys_state_get_relays(sys_state_relays_t *dst)
{
    if (dst && store_mutex && xSemaphoreTake(store_mutex, portMAX_DELAY) == pdTRUE) {
        *dst = system_state.relays;
        xSemaphoreGive(store_mutex);
    }
}

void sys_state_get_sd(sys_state_sd_t *dst)
{
    if (dst && store_mutex && xSemaphoreTake(store_mutex, portMAX_DELAY) == pdTRUE) {
        *dst = system_state.sd;
        xSemaphoreGive(store_mutex);
    }
}

void sys_state_get_wifi(sys_state_wifi_t *dst)
{
    if (dst && store_mutex && xSemaphoreTake(store_mutex, portMAX_DELAY) == pdTRUE) {
        *dst = system_state.wifi;
        xSemaphoreGive(store_mutex);
    }
}