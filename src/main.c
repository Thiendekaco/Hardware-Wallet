// main.c

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"

#include "nvs_flash.h"
#include "ssd1306.h"
#include "button_listener.h"
#include "password.h"
#include "splash_screen.h"
#include <string.h>
#include "driver/i2c.h"

static const char *TAG = "main";

// I2C pin definitions (change these if using different pins)
#define I2C_MASTER_SDA_IO    21
#define I2C_MASTER_SCL_IO    22
#define I2C_MASTER_FREQ_HZ   100000
#define I2C_MASTER_PORT      I2C_NUM_0
#define OLED_RESET_IO        -1    // If your module has no RESET pin, set to -1


// ------------------------------------------------------------------
// Task prototypes for SplashScreen initialization steps
// ------------------------------------------------------------------
static void task_displaySetup(void);
static void task_nvsInit(void);
static void task_initButtons(void);

// Array of initialization tasks passed into show_splash_screen()
static InitTask splashTasks[] = {
    task_displaySetup,
    task_nvsInit,
    task_initButtons
};

// ------------------------------------------------------------------
// task_displaySetup:
//   Clears the SSD1306 buffer. Additional splash graphics could be
//   drawn here before the display is updated.
// ------------------------------------------------------------------
static void task_displaySetup(void){}

// ------------------------------------------------------------------
// task_nvsInit:
//   Initializes NVS (non-volatile storage). If a fresh erase is needed,
//   this task performs it before returning.
// ------------------------------------------------------------------
static void task_nvsInit(void)
{
    ESP_LOGI(TAG, "RUN TASK NVS INIT");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "RUN TASK NVS END");
}

// ------------------------------------------------------------------
// task_initButtons:
//   Calls initButtons() from the ButtonListener component to
//   configure GPIOs for button inputs.
// ------------------------------------------------------------------
static void task_initButtons(void)
{    ESP_LOGI(TAG, "RUN BUTTON INIT");
     init_button_listener();
     ESP_LOGI(TAG, "RUN BUTTON INIT");
}

// ------------------------------------------------------------------
// app_main:
//   1. Log application start.
//   2. Initialize I2C bus and SSD1306 driver.
//   3. Show splash screen and run initialization tasks.
//   4. Start the PIN input flow.
//   5. Enter an infinite loop (or perform other logic).
// ------------------------------------------------------------------
void app_main(void) {
    ESP_LOGI(TAG, "=== APPLICATION START ===");

    SSD1306_t dev;
    // 1. Init core project
    ESP_LOGI(TAG, "=== APPLICATION START ===");

    // 1) Thiết lập I2C trước
    i2c_master_init(&dev, I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO, OLED_RESET_IO);
    ESP_LOGI(TAG, "=== I2C MASTER INIT ===");

    // 2) Nếu thư viện của bạn có hàm “i2c_device_add” để thêm địa chỉ 0x3C trước khi init SSD1306
    i2c_device_add(&dev, I2C_MASTER_PORT, OLED_RESET_IO, 0x3C);
    ESP_LOGI(TAG, "=== I2C ADD DEVICE INIT ===");

    // 3) Khởi tạo SSD1306: cấp phát buffer, lưu width/height, gắn bus I2C
    ssd1306_init(&dev, 128, 64);
    ESP_LOGI(TAG, "=== SSD1306 INIT ===");

    // 4) (Có thể) cần gọi thêm một hàm như i2c_init(&dev) để thiết lập page/buffer nội bộ
    i2c_init(&dev, 128, 64);
    ESP_LOGI(TAG, "=== I2C INIT ===");

    // 5) Bây giờ buffer chắc chắn đã sẵn, bus I2C đã sẵn, gọi show_splash_screen
    ESP_LOGI(TAG, "=== RUN TASK START ===");
    show_splash_screen(&dev, splashTasks, sizeof(splashTasks)/sizeof(splashTasks[0]));


    // 3. Start the PIN input workflow. This will handle showing the
    //    PIN entry UI, reading buttons, saving/verifying PIN in NVS.
    bool result = handle_password_flow(&dev);
    if (result) {
        ESP_LOGI(TAG, "Password flow completed successfully");
    } else {
        ESP_LOGW(TAG, "Password flow returned false (unexpected)");
    }

    // 4. Enter an infinite loop. Replace this with additional logic if needed.
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
