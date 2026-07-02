/**
 * @file lcd_task.h
 * @brief Header file for the OLED display task
 */

#ifndef LCD_TASK_H
#define LCD_TASK_H

#include "ssd1306_drv.h"
#include "gfx.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @struct lcd_task_args_t
 * @brief Arguments passed to the lcd_task on creation
 */
typedef struct {
    gfx_t *gfx;        ///< Graphics canvas context
    ssd1306_t *lcd;    ///< Low-level SSD1306 display device handle
} lcd_task_args_t;

/**
 * @brief FreeRTOS task function that updates the LCD display based on system state changes
 * @param pvParameters Pointer to an lcd_task_args_t structure
 */
void lcd_task(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif /* LCD_TASK_H */