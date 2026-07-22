/**
 * @file system_state.h
 * @brief Thread-safe global system state repository for aLog_project.
 *
 * Provides centralized, mutex-protected access to the device status metrics,
 * allowing safe data exchange between concurrent FreeRTOS tasks.
 */

#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

#include <stdint.h>
#include <stdbool.h>
#include "wifi_app.h" // For wifi_app_state_t

#ifdef __cplusplus
extern "C" {
#endif

/********************** Data Modules **********************/

/**
 * @struct sys_state_adc_t
 * @brief ADC telemetry data container
 */
typedef struct {
    float voltage;       /**< Calculated battery voltage in Volts (V) */
    float current;       /**< Calculated charge/discharge current in Amperes (A) */
    bool voltage_err;    /**< Battery voltage telemetry error flag */
    bool current_err;    /**< Battery current telemetry error flag */
} sys_state_adc_t;

/**
 * @struct sys_state_relays_t
 * @brief Actuator relay state container
 */
typedef struct {
    bool relay1;        /**< State of Relay 1 (true = ON, false = OFF) */
    bool relay2;        /**< State of Relay 2 (true = ON, false = OFF) */
} sys_state_relays_t;

/**
 * @struct sys_state_sd_t
 * @brief SD card storage metrics container
 */
typedef struct {
    bool err;              /**< SD Card error flag */
    bool overflow;         /**< SD Card overflow flag */
    uint32_t total_space;  /**< Total storage capacity (Mb) */
    uint32_t free_space;   /**< Available free storage space (Mb) */
} sys_state_sd_t;

/**
 * @struct sys_state_wifi_t
 * @brief 
 */
typedef struct {
    wifi_app_state_t state;
    char ip_addr[16];
} sys_state_wifi_t;

/**
 * @enum sys_time_source_t
 * @brief Source of the system clock's current synchronization
 */
typedef enum {
    TIME_SRC_UNSYNC = 0, ///< System time is unsynchronized (default 1970/2020 epoch)
    TIME_SRC_RTC,        ///< System time was restored from DS1307 RTC
    TIME_SRC_SNTP        ///< System time was synchronized with network SNTP servers
} sys_time_source_t;

/**
 * @struct sys_state_time_t
 * @brief Time sync state container
 */
typedef struct {
    sys_time_source_t source; ///< Source of truth for system time
    bool synced;              ///< Time synchronization flag
} sys_state_time_t;

/******************* Module Management ********************/

/**
 * @brief Initialize the system state module and its internal mutex
 * @note Must be called exactly once before any tasks attempt to access the state
 */
void sys_state_init(void);

/************** SETTERS (for data providers) **************/

/**
 * @brief Atomically update ADC state metrics
 * @param[in] voltage Calculated battery voltage (V)
 * @param[in] v_err   Voltage error flag (true if reading failed)
 * @param[in] current Calculated battery current (A)
 * @param[in] c_err   Current error flag (true if reading failed)
 */
void sys_state_set_adc(float voltage, bool v_err, float current, bool c_err);

/**
 * @brief Atomically update SD state metrics
 * @param[in] err         
 * @param[in] overflow    
 * @param[in] total_space Total storage size in Megabytes (Mb)
 * @param[in] free_space  Free storage size in Megabytes (Mb)
 */
void sys_state_set_sd(bool err,bool overflow, uint32_t total_space, uint32_t free_space);

/**
 * @brief Atomically update relay states
 * @param[in] r1 New state for Relay 1 (true = ON, false = OFF)
 * @param[in] r2 New state for Relay 2 (true = ON, false = OFF)
 */
void sys_state_set_relays(bool r1, bool r2);

/**
 * @brief Atomically update wi-fi states
 * @param[in] state 
 * @param[in] ip_addr 
 */
void sys_state_set_wifi(wifi_app_state_t state, const char *ip_addr);

/**
 * @brief Atomically update system time synchronization state
 * @param[in] source Current time source of truth
 * @param[in] synced True if system clock is synchronized (loaded from RTC or SNTP)
 */
void sys_state_set_time(sys_time_source_t source, bool synced);

/**************** GETTERS (for consumers) *****************/

/**
 * @brief Thread-safely copy the entire ADC telemetry block
 * @param[out] dst Pointer to the destination target structure
 */
void sys_state_get_adc(sys_state_adc_t *dst);

/**
 * @brief Thread-safely copy the entire relay state block
 * @param[out] dst Pointer to the destination target structure
 */
void sys_state_get_relays(sys_state_relays_t *dst);

/**
 * @brief Thread-safely copy the entire SD storage metrics block
 * @param[out] dst Pointer to the destination target structure
 */
void sys_state_get_sd(sys_state_sd_t *dst);

/**
 * @brief Thread-safely copy the entire wi-fi state block
 * @param[out] dst Pointer to the destination target structure
 */
void sys_state_get_wifi(sys_state_wifi_t *dst);

/**
 * @brief Thread-safely copy the current system time sync state block
 * @param[out] dst Pointer to the destination target structure
 */
void sys_state_get_time(sys_state_time_t *dst);

#ifdef __cplusplus
}
#endif

#endif /* SYSTEM_STATE_H */