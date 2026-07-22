idf_component_register(
    SRCS
        "ds1307_drv.c"

    INCLUDE_DIRS
        "."

    REQUIRES
        esp_driver_i2c

    PRIV_REQUIRES
)