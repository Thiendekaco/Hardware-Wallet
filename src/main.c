#include <stdio.h>
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "ble.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "u8g2.h"
#include "esp_log.h"
#include "esp_err.h"

#include "nvs_flash.h"
#include "button_listener.h"
#include "password.h"
#include "keyring.h"
#include "splash_screen.h"
#include "auto_sleep.h"

#define I2C_MASTER_SCL_IO           GPIO_NUM_22      // GPIO number for I2C master clock
#define I2C_MASTER_SDA_IO           GPIO_NUM_21      // GPIO number for I2C master data
#define I2C_MASTER_NUM              I2C_NUM_0 // I2C port number for master dev
#define I2C_MASTER_FREQ_HZ          400000   // I2C master clock frequency
#define I2C_MASTER_TX_BUF_DISABLE   0        // I2C master doesn't need buffer
#define I2C_MASTER_RX_BUF_DISABLE   0        // I2C master doesn't need buffer

static const char *TAG = "main";

static const uint8_t OLED_ADDR = 0x3C;

u8g2_t u8g2; // a structure which will contain all the data for one display


// ------------------------------------------------------------------
// Task prototypes for SplashScreen initialization steps
// ------------------------------------------------------------------
static void task_displaySetup(void);
static void task_nvsInit(void);
static void task_initButtons(void);
static void task_bleInit(void);

// Array of initialization tasks passed into show_splash_screen()
static InitTask splashTasks[] = {
    task_displaySetup,
    task_nvsInit,
    task_initButtons,
    task_bleInit
};

// ------------------------------------------------------------------
// task_displaySetup:
//   Clears the SSD1306 buffer. Additional splash graphics could be
//   drawn here before the display is updated.
// ------------------------------------------------------------------
static void task_displaySetup(void) {
    vTaskDelay(pdMS_TO_TICKS(5000));
}

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
// task_bleInit:
//   Initializes BLE stack and starts advertising.
// ------------------------------------------------------------------
static void task_bleInit(void)
{
    ESP_LOGI(TAG, "RUN BLE INIT");
    ESP_ERROR_CHECK(ble_init());
    xTaskCreate(ble_task, "ble_task", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "BLE INIT END");
}


// ------------------------------------------------------------------
// app_main:
//   1. Log application start.
//   2. Initialize I2C bus and SSD1306 driver.
//   3. Show splash screen and run initialization tasks.
//   4. Start the PIN input flow.
//   5. Enter an infinite loop (or perform other logic).
// ------------------------------------------------------------------

esp_err_t i2c_master_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
               .scl_pullup_en = GPIO_PULLUP_ENABLE,
               .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    esp_err_t err = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C param config failed: %s", esp_err_to_name(err));
        return err;
    }

    err = i2c_driver_install(I2C_MASTER_NUM, conf.mode,
                                 I2C_MASTER_RX_BUF_DISABLE,
                                 I2C_MASTER_TX_BUF_DISABLE, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver install failed: %s", esp_err_to_name(err));
    }
    return err;
}

// GPIO and delay function for u8g2
uint8_t u8x8_gpio_and_delay_esp32(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    switch (msg) {
        case U8X8_MSG_GPIO_AND_DELAY_INIT:
            break;
        case U8X8_MSG_DELAY_MILLI:
            vTaskDelay(pdMS_TO_TICKS(arg_int));
            break;
        case U8X8_MSG_DELAY_10MICRO:
            vTaskDelay(pdMS_TO_TICKS(0.01 * arg_int));
            break;
        case U8X8_MSG_DELAY_100NANO:
            vTaskDelay(pdMS_TO_TICKS(0.0001));
            break;
        default:
            return 0;
    }
    return 1;
}


uint8_t u8x8_byte_esp32_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
       static uint8_t buffer[32];  // static buffer
       static uint8_t buf_idx;
       uint8_t *data;

       switch (msg) {
           case U8X8_MSG_BYTE_SEND:
               data = (uint8_t *)arg_ptr;
               while (arg_int > 0) {
                   buffer[buf_idx++] = *data;
                   data++;
                   arg_int--;
               }
               break;
           case U8X8_MSG_BYTE_INIT:
               // Already initialized in i2c_master_init()
               break;
           case U8X8_MSG_BYTE_SET_DC:
               // DC (Data/Command) bit is set as part of the I2C data
               break;
           case U8X8_MSG_BYTE_START_TRANSFER:
               buf_idx = 0;
               break;
           case U8X8_MSG_BYTE_END_TRANSFER:
               i2c_master_write_to_device(I2C_MASTER_NUM, OLED_ADDR,
                                         buffer, buf_idx,
                                         1000 / portTICK_PERIOD_MS);
               break;
           default:
               return 0;
               break;
       }
       return 1;
   }
   

void u8g2_display_init(u8g2_t *pu8g2) {
    u8g2_Setup_ssd1306_i2c_128x32_univision_f(pu8g2, U8G2_R0, u8x8_byte_esp32_i2c, u8x8_gpio_and_delay_esp32);
    u8g2_InitDisplay(pu8g2);
    vTaskDelay(pdMS_TO_TICKS(100));  // Add a 100ms delay
    u8g2_SetPowerSave(pu8g2, 0);  // Wake up display
    u8g2_ClearBuffer(pu8g2);      // Clear the internal buffer
}


void app_main(void)
{
    // esp_err_t err = nvs_flash_erase();
    // if (err == ESP_OK) {
    //     printf("NVS đã được xóa sạch!\n");
    // } else {
    //     printf("Xóa NVS thất bại: %s\n", esp_err_to_name(err));
    // }
    ESP_ERROR_CHECK(i2c_master_init());

    u8g2_display_init(&u8g2);
    ESP_LOGI(TAG, "=== RUN TASK START ===");
    show_splash_screen(&u8g2, splashTasks, sizeof(splashTasks)/sizeof(splashTasks[0]));
    handle_password_flow(&u8g2);

    if (!isHaveAccount()) {
        create_account_flow();
    }

    while (1) {
        ble_status_flow();
        auto_sleep_check();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}