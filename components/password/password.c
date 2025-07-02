#include "password.h"
#include "button_listener.h"
#include "nvs.h"
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define PIN_LENGTH     4
#define NVS_NAMESPACE  "storage"
#define NVS_KEY        "pin_code"

extern u8g2_t u8g2;

void update_password(int selectedIndex, int pinIndex, int pinCode[4]) {
    u8g2_ClearBuffer(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);

    // Title
    u8g2_DrawStr(&u8g2, 30, 15, "Choose PIN");

    // Digits + delete
    const char *digits = "0123456789x";
    u8g2_DrawStr(&u8g2, 0, 35, digits);

    // Caret
    int caret_x = selectedIndex * 6;  // Based on font width
    u8g2_DrawStr(&u8g2, caret_x, 45, "_");

    // PIN display as stars
    char stars[PIN_LENGTH + 1] = {0};
    for (int i = 0; i < pinIndex; i++) {
        stars[i] = '*';
    }
    u8g2_DrawStr(&u8g2, 50, 62, stars);

    u8g2_SendBuffer(&u8g2);
}

void show_password_confirmed() {
    u8g2_ClearBuffer(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);
    u8g2_DrawStr(&u8g2, 40, 32, "PIN OK!");
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

    update_password(selectedIndex, pinIndex, pinCode);

    while (true) {
        if (is_button_left_pressed()) {
            vTaskDelay(pdMS_TO_TICKS(200));
            selectedIndex = (selectedIndex - 1 + 11) % 11;
            update_password(selectedIndex, pinIndex, pinCode);
        }
        else if (is_button_right_pressed()) {
            vTaskDelay(pdMS_TO_TICKS(200));
            selectedIndex = (selectedIndex + 1) % 11;
            update_password(selectedIndex, pinIndex, pinCode);
        }
        else if (is_button_middle_pressed()) {
            vTaskDelay(pdMS_TO_TICKS(300));

            if (selectedIndex == 10) {
                if (pinIndex > 0) {
                    pinIndex--;
                    update_password(selectedIndex, pinIndex, pinCode);
                }
            } else if (pinIndex < PIN_LENGTH) {
                pinCode[pinIndex++] = selectedIndex;
                update_password(selectedIndex, pinIndex, pinCode);

                if (pinIndex == PIN_LENGTH) {
                    if (!is_password_set()) {
                        save_pin_to_nvs(pinCode);
                        show_password_confirmed();
                        return true;
                    } else {
                        if (verify_pin(pinCode)) {
                            show_password_confirmed();
                            return true;
                        } else {
                            u8g2_ClearBuffer(&u8g2);
                            u8g2_DrawStr(&u8g2, 30, 32, "Wrong PIN!");
                            u8g2_SendBuffer(&u8g2);
                            vTaskDelay(pdMS_TO_TICKS(2000));
                            pinIndex = 0;
                            selectedIndex = 0;
                            update_password(selectedIndex, pinIndex, pinCode);
                        }
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    return false;
}
