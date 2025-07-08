idf_component_register(
        SRCS "ble.c"
        INCLUDE_DIRS "include"
        REQUIRES bt nvs_flash u8g2 button_listener ui_state keyring
)