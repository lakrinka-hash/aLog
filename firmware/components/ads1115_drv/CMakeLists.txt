idf_component_register(
    SRCS
        "ads1115_drv.c"

    INCLUDE_DIRS
        "."

    REQUIRES
        esp_driver_i2c
    
    PRIV_REQUIRES
    
)
