idf_component_register(
        SRCS "password.c"
        INCLUDE_DIRS "include"
        REQUIRES ssd1306 button_listener nvs_flash
)