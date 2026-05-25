idf_component_register(
    SRCS
        "main.c"
        "ssd1306.c"

    INCLUDE_DIRS
        "."

    REQUIRES
        esp_driver_gpio
        esp_driver_i2c
        esp_driver_spi
    
    PRIV_REQUIRES
    
)
