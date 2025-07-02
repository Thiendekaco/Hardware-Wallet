idf_component_register(
        SRCS "password.c"
        INCLUDE_DIRS "include"
        REQUIRES u8g2 button_listener nvs_flash
)