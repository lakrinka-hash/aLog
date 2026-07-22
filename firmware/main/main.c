/**
 * @file main.c
 */

#include "esp_err.h"            // for esp_err_t
#include "esp_log.h"            // for ESP_LOGI
#include "driver/gpio.h"        // for gpio_config_t
#include "driver/i2c_master.h"  // for i2c_new_master_bus, i2c_master_bus_config_t
#include "driver/spi_common.h"  // for spi_bus_initialize, spi_bus_config_t
#include "nvs_flash.h"
#include "esp_timer.h"

#include "sdkconfig.h"
#include "internal_storage.h"
#include "external_storage.h"
#include "system_state.h"
#include "system_time.h"
#include "ssd1306_drv.h"
#include "ads1115_drv.h"
#include "ds1307_drv.h"
#include "gfx.h"
#include "fonts.h"
#include "tasks_list.h"
#include "adc_task.h"
#include "lcd_task.h"
#include "log_task.h"
#include "pwr_task.h"
#include "time_task.h"
#include "udp_task.h"
#include "wifi_task.h"
#include "wifi_app.h"
#include "app_config.h"
#include "main.h" 

#define APP_DEBUG_STACK_MONITOR    0

system_task_t system_task = {0};

static i2c_master_bus_handle_t i2c_bus_handle;
static ads1115_t ads1115;
static ds1307_t ds1307;
static ssd1306_t ssd1306;
static ext_storage_t sdcard;
static gfx_t canvas;

static esp_timer_handle_t led_timer = NULL;
static volatile wifi_app_state_t s_current_wifi_state = WIFI_STATE_DISCONNECTED;
static volatile bool led_level = false;

static void wifi_state_changed_cb(wifi_app_state_t new_state) {
    ESP_LOGI("main", "Wi-Fi state changed to: %d", new_state);
    s_current_wifi_state = new_state;

    char ip_buffer[16] = "0.0.0.0";
    if (new_state == WIFI_STATE_CONNECTED) {
        wifi_app_get_ip(ip_buffer, sizeof(ip_buffer));
    }
    sys_state_set_wifi(new_state, ip_buffer);

    // Trigger timer immediately to reflect new state
    esp_timer_stop(led_timer);
    esp_timer_start_once(led_timer, 1000);
}

static void led_timer_callback(void *arg) {
    uint64_t next_period_us = 1000000; // Default (disconnected/slow)
    sys_state_wifi_t wifi_state;
    sys_state_get_wifi(&wifi_state);
    
    if (wifi_state.state == WIFI_STATE_CONNECTED) {
        // Connected: slow blink (1s period)
        led_level = !led_level;
        next_period_us = 1000000; 
    } else if (wifi_state.state == WIFI_STATE_CONNECTING || wifi_state.state == WIFI_STATE_RECONNECTING) {
        // Connecting or Reconnecting: fast blink (100ms period)
        led_level = !led_level;
        next_period_us = 100000;
    } else {
        // Disconnected (OFF)
        led_level = false;
        next_period_us = 1000000; // Just check back in 1s
    }

    gpio_set_level(CONFIG_APP_LED_PIN, led_level ? 0 : 1);
    
    esp_timer_stop(led_timer);
    esp_timer_start_once(led_timer, next_period_us);
}

static void led_init(void) {
    const esp_timer_create_args_t timer_args = {
        .callback = &led_timer_callback,
        .name = "led"
    };
    esp_timer_create(&timer_args, &led_timer);
    esp_timer_start_once(led_timer, 100000);
}

/* --------- Power CTRL ISR  ---------- */
static void IRAM_ATTR pwr_ctrl_gpio_isr_handler(void* arg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (system_task.pwr_task_handle != NULL) {
        vTaskNotifyGiveFromISR(system_task.pwr_task_handle, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken) {
            portYIELD_FROM_ISR();
        }
    }
}

/* ---- Power CTRL initialization ----- */
void pwr_ctrl_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << CONFIG_APP_PWR_CTRL_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE
    };
    gpio_config(&io_conf);

    // Register GPIO interrupt handler
    esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE("app_main", "Failed to install GPIO ISR service: %s", esp_err_to_name(err));
    }
    gpio_isr_handler_add(CONFIG_APP_PWR_CTRL_PIN, pwr_ctrl_gpio_isr_handler, NULL);
    ESP_LOGI("app_main", "Falling-edge interrupt attached on GPIO %d successfully", CONFIG_APP_PWR_CTRL_PIN);
}

/* ------ I2C bus initialization ------ */
static esp_err_t i2c_bus_init(void)
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port = CONFIG_APP_I2C_PORT,
        .sda_io_num = CONFIG_APP_I2C_SDA_PIN,
        .scl_io_num = CONFIG_APP_I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .flags.enable_internal_pullup = true
    };
    esp_err_t err;
    err = i2c_new_master_bus(&bus_config, &i2c_bus_handle);
    if (err == ESP_OK) {
        ESP_LOGI("app_main", "I2C bus (SDA=%d, SCL=%d) initialized on port: (ID=%d)",
                bus_config.sda_io_num,
                bus_config.scl_io_num,
                bus_config.i2c_port);
    }
    return err;
}

/* ------ SPI bus initialization ------ */
static esp_err_t spi_bus_init(void)
{
    spi_bus_config_t bus_config = {
        .mosi_io_num = CONFIG_APP_SPI_MOSI_PIN,
        .miso_io_num = CONFIG_APP_SPI_MISO_PIN,
        .sclk_io_num = CONFIG_APP_SPI_SCK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096
    };
    esp_err_t err;
    err = spi_bus_initialize(CONFIG_APP_SPI_HOST, &bus_config, SPI_DMA_CH_AUTO);
    if (err == ESP_OK) {
        ESP_LOGI("app_main", "SPI bus (MISO=%d, MOSI=%d, SCK=%d) initialized on host: (ID=%d)",
                bus_config.miso_io_num,
                bus_config.mosi_io_num,
                bus_config.sclk_io_num,
                CONFIG_APP_SPI_HOST);
    }
    return err;
}

/* ------- GPIOs initialization ------- */
static void gpios_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << CONFIG_APP_RLY1_PIN) | (1ULL << CONFIG_APP_RLY2_PIN) | (1ULL << CONFIG_APP_LED_PIN),
        .mode = GPIO_MODE_INPUT_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    
    gpio_set_level(CONFIG_APP_RLY1_PIN, 0);
    gpio_set_level(CONFIG_APP_RLY2_PIN, 0);
    sys_state_set_relays(false, false);
}

/* ----------- SYS MON Task ----------- */
#if APP_DEBUG_STACK_MONITOR
static void sys_mon_task(void *pvParameters)
{
    ESP_LOGI("SYS_MON", "System monitor started...");
    vTaskDelay(pdMS_TO_TICKS(5000));
    while (1)
    {
        uint32_t adc_stack = uxTaskGetStackHighWaterMark(system_task.adc_task_handle);
        uint32_t lcd_stack = uxTaskGetStackHighWaterMark(system_task.lcd_task_handle);
        uint32_t log_stack = uxTaskGetStackHighWaterMark(system_task.log_task_handle); 
        uint32_t pwr_stack = uxTaskGetStackHighWaterMark(system_task.pwr_task_handle);
        uint32_t time_stack = uxTaskGetStackHighWaterMark(system_task.time_task_handle);
        uint32_t udp_stack = uxTaskGetStackHighWaterMark(system_task.udp_task_handle);
        uint32_t wifi_stack = uxTaskGetStackHighWaterMark(system_task.wifi_task_handle);
        uint32_t mon_stack = uxTaskGetStackHighWaterMark(NULL);

        ESP_LOGI("STACK_MON", "---------- (Остаток стека) ----------");
        ESP_LOGI("STACK_MON", "adc_task:  %" PRIu32 " B", adc_stack);
        ESP_LOGI("STACK_MON", "lcd_task:  %" PRIu32 " B", lcd_stack);
        ESP_LOGI("STACK_MON", "log_task:  %" PRIu32 " B", log_stack);
        ESP_LOGI("STACK_MON", "pwr_task:  %" PRIu32 " B", pwr_stack);
        ESP_LOGI("STACK_MON", "time_task: %" PRIu32 " B", time_stack);
        ESP_LOGI("STACK_MON", "udp_task:  %" PRIu32 " B", udp_stack);
        ESP_LOGI("STACK_MON", "wifi_task: %" PRIu32 " B", wifi_stack);
        ESP_LOGI("STACK_MON", "mon_task:  %" PRIu32 " B", mon_stack);
        ESP_LOGI("STACK_MON", "-------------------------------------");

        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
#endif

void app_main(void)
{
    ESP_LOGI("app_main", "Starting system initialization...");

    // Suppress verbose Espressif Wi-Fi logs (such as DELBA session limits)
    //esp_log_level_set("wifi", ESP_LOG_WARN);

    sys_time_init();
    sys_state_init();
    pwr_ctrl_init();
    gpios_init();
    led_init();

    esp_err_t err;

    // Initialize NVS
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // Initialize Application Configuration Manager
    err = app_config_init();
    if (err != ESP_OK) {
        ESP_LOGE("app_main", "Failed to initialize Application Configuration Manager: %s", esp_err_to_name(err));
    }

    // Initialize Local Storage (SPIFFS)
    err = int_storage_mount();
    if (err != ESP_OK) {
        ESP_LOGE("app_main", "Failed to initialize and mount local SPIFFS storage!");
    }

    err = wifi_app_init();
    if (err != ESP_OK) {
        ESP_LOGE("app_main", "Failed to initialize Wi-Fi component stack");
    } else {
        wifi_app_register_state_callback(wifi_state_changed_cb);
        xTaskCreate(
            wifi_task,
            "wifi_task",
            CONFIG_APP_WIFI_TASK_STACK,
            (void *)&sdcard,
            3,
            &system_task.wifi_task_handle
        );
        xTaskCreate(
            udp_task,
            "udp_task",
            CONFIG_APP_UDP_TASK_STACK,
            NULL,
            3,
            &system_task.udp_task_handle
        );
    }

    err = i2c_bus_init();
    if (err == ESP_OK) {
        err = ssd1306_attach(i2c_bus_handle, &ssd1306, CONFIG_APP_SSD1306_ADDR);
        if (err == ESP_OK) {
            err = ssd1306_init(&ssd1306, CONFIG_APP_SSD1306_WIDTH, CONFIG_APP_SSD1306_HEIGHT, SSD1306_MODE_PAGE);
            if (err == ESP_OK) {
                err = gfx_init(&canvas, CONFIG_APP_SSD1306_WIDTH, CONFIG_APP_SSD1306_HEIGHT, &font_mono_8x8uk);
                if (err == ESP_OK) {
                    static lcd_task_args_t lcd_ctx;
                    lcd_ctx.gfx = &canvas;
                    lcd_ctx.lcd = &ssd1306;
                    xTaskCreate(
                        lcd_task,                     // Task function
                        "lcd_task",                   // Debugging name
                        CONFIG_APP_LCD_TASK_STACK,    // Stack size
                        (void *)&lcd_ctx,             // 
                        3,                            // Priority
                        &system_task.lcd_task_handle  // Descriptor
                    );
                }
            }
        }
        err = ads1115_attach(i2c_bus_handle, &ads1115, CONFIG_APP_ADS1115_ADDR);
        if (err == ESP_OK) {
            static adc_task_args_t adc_ctx;
            adc_ctx.adc = &ads1115;
            xTaskCreate(
                adc_task,                     // Task function
                "adc_task",                   // Debugging name
                CONFIG_APP_ADC_TASK_STACK,    // Stack size
                (void *)&adc_ctx,             // 
                3,                            // Priority
                &system_task.adc_task_handle  // Descriptor
            );
        }

        // Attach and register DS1307 RTC
        err = ds1307_attach(i2c_bus_handle, &ds1307, DS1307_I2C_ADDR);
        if (err == ESP_OK) {
            sys_time_register_rtc(&ds1307);

            static time_task_args_t time_ctx;
            time_ctx.rtc = &ds1307;
            xTaskCreate(
                time_task,
                "time_task",
                CONFIG_APP_TIME_TASK_STACK,
                (void *)&time_ctx,
                3,
                &system_task.time_task_handle
            );
        }
    }

    err = spi_bus_init();
    if (err == ESP_OK) {
        err = ext_storage_init(&sdcard, CONFIG_APP_SPI_HOST, CONFIG_APP_SD_CS_PIN, "/sdcard");
        if (err == ESP_OK) {
            static log_task_args_t log_ctx;
            log_ctx.storage = &sdcard;
            xTaskCreate(
                log_task,
                "log_task",
                CONFIG_APP_LOG_TASK_STACK,
                (void *)&log_ctx,
                3,
                &system_task.log_task_handle
            );
            xTaskCreate(
                pwr_task,
                "pwr_task",
                CONFIG_APP_PWR_TASK_STACK,
                NULL,
                5,
                &system_task.pwr_task_handle
            );
        }
    }

    #if APP_DEBUG_STACK_MONITOR
    xTaskCreate(sys_mon_task, "sys_mon_task", 2048, NULL, 1, NULL);
    #endif

    ESP_LOGI("app_main", "Application initialization complete. main_task exiting...");
}