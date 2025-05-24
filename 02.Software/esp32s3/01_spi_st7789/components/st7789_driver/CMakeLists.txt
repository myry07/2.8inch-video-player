idf_component_register(SRCS "my_display.c" "st7789_driver.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos esp_timer esp_lcd driver log)
