idf_component_register(
    SRCS
       "sd_spi_drv.c"

    INCLUDE_DIRS
        "."

    REQUIRES
        esp_driver_spi
        sdmmc
    
    PRIV_REQUIRES
        fatfs
        vfs
)