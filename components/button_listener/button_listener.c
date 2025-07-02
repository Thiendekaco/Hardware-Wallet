#include "button_listener.h"

// Khai báo chân GPIO theo ESP-IDF
const gpio_num_t BUTTON_LEFT = GPIO_NUM_17;
const gpio_num_t BUTTON_RIGHT = GPIO_NUM_4;
const gpio_num_t BUTTON_MIDDLE = GPIO_NUM_16;

void init_button_listener() {
    // Cấu hình chân GPIO làm input với pull-up nội bộ
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_LEFT) | (1ULL << BUTTON_RIGHT) | (1ULL << BUTTON_MIDDLE),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
}

bool is_button_left_pressed() {
    // Chân kéo lên (HIGH) khi chưa nhấn, kéo xuống (LOW) khi nhấn
    return gpio_get_level(BUTTON_LEFT) == 0;
}

bool is_button_right_pressed() {
    return gpio_get_level(BUTTON_RIGHT) == 0;
}

bool is_button_middle_pressed() {
    return gpio_get_level(BUTTON_MIDDLE) == 0;
}
