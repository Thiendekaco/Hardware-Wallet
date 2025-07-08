#include "auto_sleep.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "u8g2.h"
#include "ble.h"

#define AUTO_SLEEP_TIMEOUT_MS 300000 // 5 minutes
static const char *TAG = "auto_sleep";
static uint32_t last_activity = 0;
static bool is_sleeping = false;
static uint64_t wakeup_mask = 0;
extern u8g2_t u8g2;

void auto_sleep_init(uint64_t mask) {
    last_activity = esp_log_timestamp();
    wakeup_mask = mask;
}

void auto_sleep_record_activity(void) {
    last_activity = esp_log_timestamp();
    if (is_sleeping) {
        u8g2_SetPowerSave(&u8g2, 0);
        ble_start_advertising();
        is_sleeping = false;
    }
}

static void enter_sleep(void) {
    ble_disconnect();
    ble_stop_advertising();
    u8g2_SetPowerSave(&u8g2, 1);

    if (wakeup_mask) {
        esp_sleep_enable_ext1_wakeup(wakeup_mask, ESP_EXT1_WAKEUP_ALL_LOW);
    }
    is_sleeping = true;
    ESP_LOGI(TAG, "Entering light sleep");
    esp_light_sleep_start();
    ESP_LOGI(TAG, "Woke from sleep");
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    u8g2_SetPowerSave(&u8g2, 0);
    ble_start_advertising();
    is_sleeping = false;
}

void auto_sleep_check(void) {
    if (!is_sleeping) {
        uint32_t now = esp_log_timestamp();
        if ((now - last_activity) > AUTO_SLEEP_TIMEOUT_MS) {
            enter_sleep();
            last_activity = esp_log_timestamp();
        }
    }
}
