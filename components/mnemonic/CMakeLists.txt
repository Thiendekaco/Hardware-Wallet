idf_component_register(
        SRCS "mnemonic.c"
        INCLUDE_DIRS "include"
        REQUIRES crypto u8g2 button_listener
)