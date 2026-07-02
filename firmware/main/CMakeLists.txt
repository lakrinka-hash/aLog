idf_component_register(
    SRCS
        "main.c"
        "system_tasks/adc_task.c"
        "system_tasks/lcd_task.c"
        "system_tasks/log_task.c"
        "system_tasks/pwr_task.c"
        "system_tasks/udp_task.c"
        "system_tasks/wifi_task.c"
        "system_storage.c"
        "system_state.c"
        "system_time.c"
        "web_server.c"

    INCLUDE_DIRS
        "system_tasks"
        "."

    REQUIRES

    PRIV_REQUIRES
        esp_driver_gpio
        esp_driver_i2c
        esp_driver_spi
        esp_event
        esp_timer
        esp_wifi
        esp_netif
        mdns
        esp_http_server
        wifi_app
        nvs_flash
        sd_spi_drv
        ssd1306_drv
        ads1115_drv
        mono_fonts
        gfx
)
