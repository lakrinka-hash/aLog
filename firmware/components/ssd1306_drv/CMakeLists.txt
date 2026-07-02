idf_component_register(
    SRCS
        "ssd1306_drv.c"

    INCLUDE_DIRS
        "."

    REQUIRES
        esp_driver_i2c
        mono_fonts
    
    PRIV_REQUIRES
    
)