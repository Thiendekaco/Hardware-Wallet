#pragma once

#ifndef PASSWORD_H
#define PASSWORD_H

#include <stdbool.h>
#include "u8g2.h"

// Extern display instance (defined in main.c)
extern u8g2_t u8g2;

// Display updated password UI
void update_password(int selectedIndex, int pinIndex, int pinCode[4]);

// Show PIN OK confirmation
void show_password_confirmed();

// Save the PIN to NVS
void save_pin_to_nvs(const int pinCode[4]);

// Check if a PIN exists in NVS
bool is_password_set();

// Verify the entered PIN
bool verify_pin(const int pinCode[4]);

// Full PIN input flow
bool handle_password_flow();

#endif
