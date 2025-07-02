#include <stdio.h>
#include "driver/gpio.h"
#include "driver/i2c_master.h" 
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
#include "splash_screen.h"
#include "driver/i2c.h"

#define I2C_MASTER_SCL_IO           GPIO_NUM_22      // GPIO number for I2C master clock
#define I2C_MASTER_SDA_IO           GPIO_NUM_21      // GPIO number for I2C master data
#define I2C_MASTER_NUM              I2C_NUM_0 // I2C port number for master dev
#define I2C_MASTER_FREQ_HZ          400000   // I2C master clock frequency
#define I2C_MASTER_TX_BUF_DISABLE   0        // I2C master doesn't need buffer
#define I2C_MASTER_RX_BUF_DISABLE   0        // I2C master doesn't need buffer

static const char *TAG = "main";

i2c_master_bus_handle_t i2c_bus = NULL;
i2c_master_dev_handle_t i2c_dev = NULL;

u8g2_t u8g2; // a structure which will contain all the data for one display


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

esp_err_t i2c_master_init(void)
{
    i2c_master_bus_config_t i2c_bus_config = {
        .i2c_port = -1,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 1,
        .flags = {
            .enable_internal_pullup = true
        }
    };
    esp_err_t err = i2c_new_master_bus(&i2c_bus_config, &i2c_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus initialization failed");
    }

    i2c_device_config_t i2c_dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x3C,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
        .scl_wait_us = 1000,
        .flags = {
            .disable_ack_check = false
        }
    };

    i2c_master_bus_add_device(i2c_bus, &i2c_dev_config, &i2c_dev);
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
               i2c_master_transmit(i2c_dev, buffer, buf_idx, 1000 / portTICK_PERIOD_MS);
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
    ESP_ERROR_CHECK(i2c_master_init());

    u8g2_display_init(&u8g2);
    ESP_LOGI(TAG, "=== RUN TASK START ===");
    show_splash_screen(&u8g2, splashTasks, sizeof(splashTasks)/sizeof(splashTasks[0]));

    bool result = handle_password_flow(&u8g2);
    if (result) {
        ESP_LOGI(TAG, "Password flow completed successfully");
    } else {
        ESP_LOGW(TAG, "Password flow returned false (unexpected)");
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}