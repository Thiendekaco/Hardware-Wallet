#include "password.h"
#include "button_listener.h"    // To use the button functions
#include "nvs.h"
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define PIN_LENGTH     4
#define NVS_NAMESPACE  "storage"
#define NVS_KEY        "pin_code"

/*
 * Initialize the PIN selection screen:
 * - Clear the display buffer
 * - Show the text "Choose PIN:" on the first line
 */
void init_password(SSD1306_t *dev) {
    ssd1306_clear_screen(dev, false);
    ssd1306_display_text(dev, 0, "Choose PIN:", 11, false);
}

/*
 * Update the password UI whenever selectedIndex or pinIndex changes:
 * - Clear the screen
 * - Draw "Choose PIN:" again
 * - Draw digits 0–9 horizontally
 * - Draw an underline beneath the currently selected digit
 * - Show '*' for each digit already entered
 */
void update_password(SSD1306_t *dev, int selectedIndex, int pinIndex, int pinCode[4]) {
    ssd1306_clear_screen(dev, false);
    ssd1306_display_text(dev, 0, "Choose PIN:", 11, false);

    // Build a string "0 1 2 3 4 5 6 7 8 9"
    char digits[21] = {0};
    for (int i = 0; i < 10; i++) {
        digits[i * 2]     = '0' + i;
        digits[i * 2 + 1] = ' ';
    }
    ssd1306_display_text(dev, 2, digits, strlen(digits), false);

    // Show asterisks for entered digits
    char stars[PIN_LENGTH * 2 + 1] = {0};
    for (int i = 0; i < pinIndex; i++) {
        stars[i * 2]     = '*';
        stars[i * 2 + 1] = ' ';
    }
    ssd1306_display_text(dev, 4, stars, strlen(stars), false);

    // Draw underline under the selected digit
    // Assuming each digit is approximately 12 pixels wide, draw line from x to x+6
    int underlineX = selectedIndex * 12;
    _ssd1306_line(dev, underlineX, 30, underlineX + 6, 30, true);

    // Finally, send the buffer to the display
    ssd1306_show_buffer(dev);
}

/*
 * Show "PIN OK!" confirmation for 1.5 seconds
 */
void show_password_confirmed(SSD1306_t *dev) {
    ssd1306_clear_screen(dev, false);
    ssd1306_display_text(dev, 3, "PIN OK!", 7, false);
    vTaskDelay(pdMS_TO_TICKS(1500));
}

/*
 * Save a 4-byte PIN array into NVS as a blob
 */
void save_pin_to_nvs(const int pinCode[4]) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        // Store the PIN blob
        err = nvs_set_blob(handle, NVS_KEY, pinCode, PIN_LENGTH * sizeof(int));
        if (err == ESP_OK) {
            nvs_commit(handle);
        }
        nvs_close(handle);
    }
}

/*
 * Check if NVS already contains a PIN:
 * - Attempt to read the blob size under key "pin_code"
 * - If it exists and its size matches 4*sizeof(int), a PIN is set
 */
bool is_password_set() {
    nvs_handle_t handle;
    size_t required_size = 0;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) return false;

    err = nvs_get_blob(handle, NVS_KEY, NULL, &required_size);
    nvs_close(handle);
    return (err == ESP_OK && required_size == PIN_LENGTH * sizeof(int));
}

/*
 * Verify the entered PIN:
 * - Read the stored PIN blob from NVS into storedPin[4]
 * - Compare it against the pinCode array passed in
 */
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

/*
 * Manage the full PIN input flow:
 * 1. Call init_password() and update_password() to render the initial interface.
 * 2. In an infinite loop, monitor three buttons:
 *    - Left  => decrement selectedIndex
 *    - Right => increment selectedIndex
 *    - Middle => select the current digit and add it to pinCode[]
 * 3. Once 4 digits are entered:
 *    - If no PIN exists yet: save the new PIN, show confirmation, return true
 *    - If a PIN exists: call verify_pin()
 *        + If correct: show confirmation, return true
 *        + If incorrect: display "Wrong PIN!", reset pinIndex and selectedIndex, redraw interface
 */
bool handle_password_flow(SSD1306_t *dev) {
    int selectedIndex = 0;
    int pinCode[PIN_LENGTH] = {0};
    int pinIndex = 0;

    init_password(dev);
    update_password(dev, selectedIndex, pinIndex, pinCode);

    while (true) {
        if (is_button_left_pressed()) {
            vTaskDelay(pdMS_TO_TICKS(200));
            selectedIndex = (selectedIndex - 1 + 10) % 10;
            update_password(dev, selectedIndex, pinIndex, pinCode);
        }
        else if (is_button_right_pressed()) {
            vTaskDelay(pdMS_TO_TICKS(200));
            selectedIndex = (selectedIndex + 1) % 10;
            update_password(dev, selectedIndex, pinIndex, pinCode);
        }
        else if (is_button_middle_pressed()) {
            vTaskDelay(pdMS_TO_TICKS(300));
            if (pinIndex < PIN_LENGTH) {
                pinCode[pinIndex++] = selectedIndex;
                update_password(dev, selectedIndex, pinIndex, pinCode);

                if (pinIndex == PIN_LENGTH) {
                    if (!is_password_set()) {
                        save_pin_to_nvs(pinCode);
                        show_password_confirmed(dev);
                        return true;
                    } else {
                        if (verify_pin(pinCode)) {
                            show_password_confirmed(dev);
                            return true;
                        } else {
                            ssd1306_clear_screen(dev, false);
                            ssd1306_display_text(dev, 3, "Wrong PIN!", 10, false);
                            vTaskDelay(pdMS_TO_TICKS(2000));
                            // Reset to allow re-entry
                            pinIndex = 0;
                            selectedIndex = 0;
                            update_password(dev, selectedIndex, pinIndex, pinCode);
                        }
                    }
                }
            }
        }

        // Light delay to reduce CPU usage; check again after 50 ms
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    // This line is never reached
    return false;
}
