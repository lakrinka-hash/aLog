/**
 * @file main.h
 * @brief Project-wide configuration macros and fallback definitions.
 * 
 * @details This file contains default configuration macros and is necessary
 *          for a successful project build in environments that do not use Kconfig
 *          (or when compiling without the generated sdkconfig configuration file).
 */

#ifndef MAIN_H
#define MAIN_H

/************* Configuration Source Check **************/

#if __has_include("sdkconfig.h")
#include "sdkconfig.h"
#else
#warning "================================================"
#warning "|         Kconfig/sdkconfig not found!         |"
#warning "|   Falling back to projdef.h default values   |"
#warning "================================================"
#endif

/*********** Device Connection Configuration ***********/

#ifndef CONFIG_APP_I2C_SDA_PIN
#define CONFIG_APP_I2C_SDA_PIN       10  ///< 
#endif

#ifndef CONFIG_APP_I2C_SCL_PIN
#define CONFIG_APP_I2C_SCL_PIN       9   ///< 
#endif

#ifndef CONFIG_APP_I2C_PORT
#define CONFIG_APP_I2C_PORT          0   ///< 
#endif

#ifndef CONFIG_APP_SPI_MOSI_PIN
#define CONFIG_APP_SPI_MOSI_PIN      7   ///< 
#endif

#ifndef CONFIG_APP_SPI_MISO_PIN
#define CONFIG_APP_SPI_MISO_PIN      5   ///< 
#endif

#ifndef CONFIG_APP_SPI_SCK_PIN
#define CONFIG_APP_SPI_SCK_PIN       6   ///< 
#endif

#ifndef CONFIG_APP_SPI_HOST
#define CONFIG_APP_SPI_HOST          1   ///< 
#endif

#ifndef CONFIG_APP_SD_CS_PIN
#define CONFIG_APP_SD_CS_PIN         4   ///< 
#endif

#ifndef CONFIG_APP_ADS1115_DRDY_PIN
#define CONFIG_APP_ADS1115_DRDY_PIN  1   ///< 
#endif

#ifndef CONFIG_APP_PWR_CTRL_PIN
#define CONFIG_APP_PWR_CTRL_PIN      0   ///< 
#endif

#ifndef CONFIG_APP_LED_PIN
#define CONFIG_APP_LED_PIN           8   ///< 
#endif

#ifndef CONFIG_APP_RLY1_PIN
#define CONFIG_APP_RLY1_PIN          2   ///< 
#endif

#ifndef CONFIG_APP_RLY2_PIN
#define CONFIG_APP_RLY2_PIN          3   ///< 
#endif

/************ Device Hardware Configuration ************/

#ifndef CONFIG_APP_SSD1306_ADDR
#define CONFIG_APP_SSD1306_ADDR    0x3C  ///< 
#endif

#ifndef CONFIG_APP_SSD1306_WIDTH
#define CONFIG_APP_SSD1306_WIDTH   128   ///< 
#endif

#ifndef CONFIG_APP_SSD1306_HEIGHT
#define CONFIG_APP_SSD1306_HEIGHT  32    ///< 
#endif

#ifndef CONFIG_APP_ADS1115_ADDR
#define CONFIG_APP_ADS1115_ADDR    0x48  ///< 
#endif

/************ Device Software Configuration ************/

#ifndef CONFIG_APP_ADS1115_RATE
#define CONFIG_APP_ADS1115_RATE  0x0080  ///< ADS1115 Data Rate Setting 128 SPS
#endif

#ifndef CONFIG_APP_ADS1115_PGA
#define CONFIG_APP_ADS1115_PGA   0x0200  ///< ADS1115 Programmable Gain Amplifier 4096 mV
#endif

/*********** Modules Software Configuration ************/

#ifndef CONFIG_APP_SD_LOG_BUFFER_SIZE
#define CONFIG_APP_SD_LOG_BUFFER_SIZE     4096  ///< 
#endif

#ifndef CONFIG_APP_SD_FLUSH_INTERVAL_SEC
#define CONFIG_APP_SD_FLUSH_INTERVAL_SEC  10    ///< 
#endif

#ifndef CONFIG_APP_MAX_UNSYNCED_FILES
#define CONFIG_APP_MAX_UNSYNCED_FILES     100   ///< 
#endif

/************** System Task Configuration **************/

#ifndef CONFIG_APP_ADC_TASK_STACK
#define CONFIG_APP_ADC_TASK_STACK        1536
#endif

#ifndef CONFIG_APP_LCD_TASK_STACK
#define CONFIG_APP_LCD_TASK_STACK        1792
#endif

#ifndef CONFIG_APP_LOG_TASK_STACK
#define CONFIG_APP_LOG_TASK_STACK        3328
#endif

#ifndef CONFIG_APP_PWR_TASK_STACK
#define CONFIG_APP_PWR_TASK_STACK        1536
#endif

#ifndef CONFIG_APP_UDP_TASK_STACK
#define CONFIG_APP_UDP_TASK_STACK        1792
#endif

#ifndef CONFIG_APP_WIFI_TASK_STACK
#define CONFIG_APP_WIFI_TASK_STACK       2048
#endif

#endif /* MAIN_H */