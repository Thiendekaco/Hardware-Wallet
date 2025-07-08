#include "ui_state.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

volatile bool g_ui_busy = false;

void ui_set_busy(bool busy) {
    g_ui_busy = busy;
}

bool ui_is_busy(void) {
    return g_ui_busy;
}

void ui_wait_until_free(void) {
    while (g_ui_busy) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}