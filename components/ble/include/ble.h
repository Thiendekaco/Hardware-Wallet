#ifndef BLE_H
#define BLE_H

#include "esp_bt_device.h"
#include <string.h>
#include <stdbool.h>

// BLE Connection status
#define BLE_CONNECTED      1
#define BLE_DISCONNECTED   0

// Session timeout (milliseconds)
#define BLE_SESSION_TIMEOUT_MS  60000

// BLE Session Info Structure
typedef struct {
 uint8_t session_id[16];        // Session ID
 uint32_t last_activity_time;   // Timestamp of last activity
} ble_session_info_t;

// Initialize BLE stack and register callbacks
esp_err_t ble_init(void);

// Start/stop advertising
esp_err_t ble_start_advertising(void);
esp_err_t ble_stop_advertising(void);

// Sending data via BLE indication (notification)
esp_err_t ble_send_data(uint8_t* data, size_t length);

// Check and handle session timeout (called periodically)
void ble_session_timeout_check(void);

// BLE management task (for timeout or other periodic checks)
void ble_task(void* param);

// Accessors for connection state and session info (optional)
uint8_t ble_get_connection_status(void);

const ble_session_info_t* ble_get_session_info(void);

// Display connection status and handle disconnect option
void ble_status_flow(void);

#endif // BLE_H
