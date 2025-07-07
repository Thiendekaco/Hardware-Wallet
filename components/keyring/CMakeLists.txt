idf_component_register(
        SRCS "keyring.c"
        INCLUDE_DIRS "include"
        REQUIRES crypto u8g2 button_listener nvs_flash mnemonic password
)