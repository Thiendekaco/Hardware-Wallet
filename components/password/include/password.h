#pragma once

#ifndef PASSWORD_H
#define PASSWORD_H

#include "ssd1306.h"

// Initialize the PIN selection interface
void init_password(SSD1306_t *dev);

// Update the UI when a user selects a digit
void update_password(SSD1306_t *dev, int selectedIndex, int pinIndex, int pinCode[4]);

// Display confirmation message when the correct PIN is entered
void show_password_confirmed(SSD1306_t *dev);

// Save the PIN to NVS
void save_pin_to_nvs(const int pinCode[4]);

// Check if a PIN is already stored
bool is_password_set();

// Verify if the entered PIN matches the stored PIN
bool verify_pin(const int pinCode[4]);

// Handle the complete PIN input flow
bool handle_password_flow(SSD1306_t *dev);

#endif
