#ifndef BUTTON_LISTENER_H
#define BUTTON_LISTENER_H

#include "driver/gpio.h"

// Declare constants for the button pins
extern const gpio_num_t BUTTON_LEFT;
extern const gpio_num_t BUTTON_RIGHT;
extern const gpio_num_t BUTTON_MIDDLE;

void init_button_listener();
bool is_button_left_pressed();
bool is_button_right_pressed();
bool is_button_middle_pressed();

#endif
