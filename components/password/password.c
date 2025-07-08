#include "password.h"
#include "button_listener.h"
#include "nvs.h"
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ui_state.h"

#define PIN_LENGTH     4
#define NVS_NAMESPACE  "storage"
#define NVS_KEY        "pin_code"
#define MAX_ATTEMPTS   5

extern u8g2_t u8g2;

void draw_star_pixel(int x, int y) {
    u8g2_DrawPixel(&u8g2, x, y);         // center
    u8g2_DrawPixel(&u8g2, x - 1, y);     // left
    u8g2_DrawPixel(&u8g2, x + 1, y);     // right
    u8g2_DrawPixel(&u8g2, x, y - 1);     // top
    u8g2_DrawPixel(&u8g2, x, y + 1);     // bottom
}

void draw_pin_stars(int pinIndex) {
    int start_x = 20;
    int y = 28;
    for (int i = 0; i < pinIndex; i++) {
        draw_star_pixel(start_x + i * 8, y);  // 8px spacing
    }
}


void update_password(int selectedIndex, int pinIndex, int pinCode[4]) {
    u8g2_ClearBuffer(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_profont10_tf);

    // Title
    u8g2_DrawStr(&u8g2, 40, 8, "Choose Pin");

    u8g2_SetFont(&u8g2, u8g2_font_micro_mr);
    // Digit row with spacing
    const char *digits = "0 1 2 3 4 5 6 7 8 9 <";
    u8g2_DrawStr(&u8g2, 20, 20, digits);

    // Caret under selected digit
    int caret_x = (selectedIndex * 8) + 20;
    u8g2_DrawStr(&u8g2, caret_x, 25, "-");

    // PIN display as spaced stars
    draw_pin_stars(pinIndex);

    u8g2_SendBuffer(&u8g2);
}

void show_password_confirmed() {
    u8g2_ClearBuffer(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_profont10_tf);
    u8g2_DrawStr(&u8g2, 45, 32, "PIN OK!");
    u8g2_SendBuffer(&u8g2);
    vTaskDelay(pdMS_TO_TICKS(1500));
}

void save_pin_to_nvs(const int pinCode[4]) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        err = nvs_set_blob(handle, NVS_KEY, pinCode, PIN_LENGTH * sizeof(int));
        if (err == ESP_OK) {
            nvs_commit(handle);
        }
        nvs_close(handle);
    }
}

bool is_password_set() {
    nvs_handle_t handle;
    size_t required_size = 0;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) return false;

    err = nvs_get_blob(handle, NVS_KEY, NULL, &required_size);
    nvs_close(handle);
    return (err == ESP_OK && required_size == PIN_LENGTH * sizeof(int));
}

bool verify_pin(const int pinCode[4]) {
    int storedPin[PIN_LENGTH];
    size_t size = sizeof(storedPin);

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) return false;

    err = nvs_get_blob(handle, NVS_KEY, storedPin, &size);
    nvs_close(handle);

    if (err != ESP_OK || size != sizeof(storedPin)) {
        return false;
    }
    return memcmp(storedPin, pinCode, sizeof(storedPin)) == 0;
}

bool handle_password_flow() {
    int selectedIndex = 0;
    int pinCode[PIN_LENGTH] = {0};
    int pinIndex = 0;

    int confirmPin[PIN_LENGTH] = {0};
    int confirmPinIndex = 0;

    bool need_confirm = false;    // Flag for confirming new PIN
    int failed_attempts = 0;      // Counter for failed attempts

    update_password(selectedIndex, pinIndex, pinCode);

    // Check if password is already set in NVS
    bool password_set = is_password_set();

    while (true) {
        ui_wait_until_free();
        if (is_button_left_pressed()) {
            vTaskDelay(pdMS_TO_TICKS(400));
            selectedIndex = (selectedIndex - 1 + 11) % 11;
            update_password(selectedIndex, need_confirm ? confirmPinIndex : pinIndex, need_confirm ? confirmPin : pinCode);
        }
        else if (is_button_right_pressed()) {
            vTaskDelay(pdMS_TO_TICKS(400));
            selectedIndex = (selectedIndex + 1) % 11;
            update_password(selectedIndex, need_confirm ? confirmPinIndex : pinIndex, need_confirm ? confirmPin : pinCode);
        }
        else if (is_button_middle_pressed()) {
            vTaskDelay(pdMS_TO_TICKS(400));

            // Handle backspace
            if (selectedIndex == 10) {
                if (need_confirm) {
                    if (confirmPinIndex > 0) confirmPinIndex--;
                } else {
                    if (pinIndex > 0) pinIndex--;
                }
            }
            // Handle digit input
            else {
                if (!password_set) {
                    // -------- New PIN setting flow --------
                    if (!need_confirm) {
                        if (pinIndex < PIN_LENGTH) {
                            pinCode[pinIndex++] = selectedIndex;
                            update_password(selectedIndex, pinIndex, pinCode);

                            if (pinIndex == PIN_LENGTH) {
                                // First entry done, ask for confirmation
                                need_confirm = true;
                                confirmPinIndex = 0;
                                memset(confirmPin, 0, sizeof(confirmPin));

                                // Show confirm prompt
                                u8g2_ClearBuffer(&u8g2);
                                u8g2_SetFont(&u8g2, u8g2_font_profont10_tf);
                                u8g2_DrawStr(&u8g2, 14, 32, "Confirm PIN again");
                                u8g2_SendBuffer(&u8g2);
                                vTaskDelay(pdMS_TO_TICKS(1200));
                                update_password(selectedIndex, confirmPinIndex, confirmPin);
                            }
                        }
                    } else {
                        // -------- Confirm new PIN flow --------
                        if (confirmPinIndex < PIN_LENGTH) {
                            confirmPin[confirmPinIndex++] = selectedIndex;
                            update_password(selectedIndex, confirmPinIndex, confirmPin);

                            if (confirmPinIndex == PIN_LENGTH) {
                                // Compare PINs
                                if (memcmp(pinCode, confirmPin, sizeof(pinCode)) == 0) {
                                    // PIN confirmed, save to NVS
                                    save_pin_to_nvs(pinCode);
                                    show_password_confirmed();
                                    return true;
                                } else {
                                    // PINs do not match, show error and restart
                                    u8g2_ClearBuffer(&u8g2);
                                    u8g2_SetFont(&u8g2, u8g2_font_profont10_tf);
                                    u8g2_DrawStr(&u8g2, 10, 32, "PIN not match!");
                                    u8g2_SendBuffer(&u8g2);
                                    vTaskDelay(pdMS_TO_TICKS(2000));

                                    // Reset state to enter PIN again
                                    pinIndex = 0;
                                    confirmPinIndex = 0;
                                    need_confirm = false;
                                    memset(pinCode, 0, sizeof(pinCode));
                                    memset(confirmPin, 0, sizeof(confirmPin));
                                    update_password(selectedIndex, pinIndex, pinCode);
                                }
                            }
                        }
                    }
                } else {
                    // -------- Existing PIN verification flow --------
                    if (pinIndex < PIN_LENGTH) {
                        pinCode[pinIndex++] = selectedIndex;
                        update_password(selectedIndex, pinIndex, pinCode);

                        if (pinIndex == PIN_LENGTH) {
                            if (verify_pin(pinCode)) {
                                show_password_confirmed();
                                return true;
                            } else {
                                failed_attempts++;  // Increment failed attempt counter

                                // Show error message
                                u8g2_ClearBuffer(&u8g2);
                                u8g2_SetFont(&u8g2, u8g2_font_profont10_tf);
                                u8g2_DrawStr(&u8g2, 35, 32, "Wrong PIN!");
                                u8g2_SendBuffer(&u8g2);
                                vTaskDelay(pdMS_TO_TICKS(2000));

                                // Check if max attempts reached
                                if (failed_attempts >= MAX_ATTEMPTS) {
                                    u8g2_ClearBuffer(&u8g2);
                                    u8g2_SetFont(&u8g2, u8g2_font_profont10_tf);
                                    u8g2_DrawStr(&u8g2, 20, 32, "Too many failed attempts.");
                                    u8g2_SendBuffer(&u8g2);
                                    vTaskDelay(pdMS_TO_TICKS(2000));
                                    return false;  // Return false if failed 5 times
                                }

                                // Reset to re-enter PIN
                                pinIndex = 0;
                                memset(pinCode, 0, sizeof(pinCode));
                                update_password(selectedIndex, pinIndex, pinCode);
                            }
                        }
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

