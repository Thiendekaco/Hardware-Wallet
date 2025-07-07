#include "ble.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "string.h"
#include "esp_random.h"

static const char* TAG = "BLE_Component";

static uint8_t is_connected = BLE_DISCONNECTED;
static ble_session_info_t current_session;

#define PROFILE_APP_ID 0
static uint16_t gatts_if_handle = 0;
static uint16_t conn_id = 0;
static uint16_t service_handle = 0;
static uint16_t char_handle = 0;
static uint16_t cccd_handle = 0; // Client Characteristic Configuration Descriptor (enable notify)

#define RECV_BUFFER_SIZE 256
static uint8_t recv_buffer[RECV_BUFFER_SIZE];
static size_t recv_len = 0;

static bool notify_enabled = false;

// UUID 128-bit example (custom service & characteristic)
static const uint8_t service_uuid[16] = {
    0x12,0x34,0x56,0x78,0x9a,0xbc,0xde,0xf0,
    0x12,0x34,0x56,0x78,0x9a,0xbc,0xde,0xf0
};
static const uint8_t char_uuid[16] = {
    0xab,0xcd,0xef,0x01,0x23,0x45,0x67,0x89,
    0xab,0xcd,0xef,0x01,0x23,0x45,0x67,0x89
};

// Advertising parameters
static esp_ble_adv_params_t adv_params = {
    .adv_int_min        = 0x100,
    .adv_int_max        = 0x200,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};
static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = false,
    .appearance = 0x00,
    .manufacturer_len = 0,
    .p_manufacturer_data = NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = 16,
    .p_service_uuid = (uint8_t*)service_uuid,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

// Permissions and properties
#define CHAR_PERM (ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE)
#define CHAR_PROP (ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY)

static void ble_gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch(event) {
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
                ESP_LOGI(TAG, "Advertising started");
            }
            break;

        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
            if (param->adv_stop_cmpl.status == ESP_BT_STATUS_SUCCESS) {
                ESP_LOGI(TAG, "Advertising stopped");
            }
            break;

        default:
            break;
    }
}

static void ble_gatt_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    esp_err_t ret;

    switch(event) {
        case ESP_GATTS_REG_EVT: {
            gatts_if_handle = gatts_if;  // Lấy từ tham số hàm

            // Tạo service
            esp_gatt_srvc_id_t service_id = {
                .is_primary = true,
                .id.inst_id = 0,
                .id.uuid.len = ESP_UUID_LEN_128,
            };
            memcpy(service_id.id.uuid.uuid.uuid128, service_uuid, 16);

            ret = esp_ble_gatts_create_service(gatts_if_handle, &service_id, 4); // max handle 4
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Create service failed %d", ret);
            }
            break;
        }

        case ESP_GATTS_CREATE_EVT: {
            service_handle = param->create.service_handle;

            // Thêm characteristic
            esp_bt_uuid_t char_uuid_struct = {
                .len = ESP_UUID_LEN_128,
            };
            memcpy(char_uuid_struct.uuid.uuid128, char_uuid, 16);

            ret = esp_ble_gatts_add_char(service_handle, &char_uuid_struct,
                                         CHAR_PERM, CHAR_PROP,
                                         NULL, NULL);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Add char failed %d", ret);
            }
            break;
        }

        case ESP_GATTS_ADD_CHAR_EVT: {
            char_handle = param->add_char.attr_handle;

            // Thêm CCCD (client config descriptor) để client bật/tắt notify
            esp_bt_uuid_t descr_uuid = {
                .len = ESP_UUID_LEN_16,
                .uuid = {.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG},
            };

            ret = esp_ble_gatts_add_char_descr(service_handle, &descr_uuid,
                                              ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                              NULL, NULL);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Add char descr failed %d", ret);
            }
            break;
        }

        case ESP_GATTS_ADD_CHAR_DESCR_EVT: {
            cccd_handle = param->add_char_descr.attr_handle;

            // Bắt đầu service
            esp_ble_gatts_start_service(service_handle);
            break;
        }

        case ESP_GATTS_CONNECT_EVT: {
            is_connected = BLE_CONNECTED;
            conn_id = param->connect.conn_id;

            for (int i = 0; i < 16; i++) {
                current_session.session_id[i] = esp_random() & 0xFF;
            }
            current_session.last_activity_time = esp_log_timestamp();
            break;
        }

        case ESP_GATTS_DISCONNECT_EVT: {
            is_connected = BLE_DISCONNECTED;
            notify_enabled = false;
            esp_ble_gap_start_advertising(&adv_params);
            break;
        }

        case ESP_GATTS_WRITE_EVT: {
            current_session.last_activity_time = esp_log_timestamp();

            if (!param->write.is_prep) {
                if (param->write.handle == char_handle) {
                    size_t copy_len = param->write.len > RECV_BUFFER_SIZE ? RECV_BUFFER_SIZE : param->write.len;
                    memcpy(recv_buffer, param->write.value, copy_len);
                    recv_len = copy_len;

                    // XỬ LÝ DỮ LIỆU NHẬN ĐƯỢC NGAY TẠI ĐÂY

                    // Gửi phản hồi OK nếu client cần
                    if (param->write.need_rsp) {
                        esp_ble_gatts_send_response(gatts_if_handle, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
                    }
                }
                else if (param->write.handle == cccd_handle) {
                    // CCCD (enable/disable notify)
                    if (param->write.len == 2) {
                        uint16_t descr_value = param->write.value[0] | (param->write.value[1] << 8);
                        notify_enabled = (descr_value == 0x0001);
                    }
                    if (param->write.need_rsp) {
                        esp_ble_gatts_send_response(gatts_if_handle, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
                    }
                }
            }
            break;
        }

        case ESP_GATTS_READ_EVT: {
            if (param->read.handle == char_handle) {
                esp_gatt_rsp_t rsp = {0};
                rsp.attr_value.len = recv_len;
                memcpy(rsp.attr_value.value, recv_buffer, recv_len);
                esp_ble_gatts_send_response(gatts_if_handle, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &rsp);
            }
            break;
        }

        default:
            break;
    }
}

esp_err_t ble_init(void) {
    esp_err_t ret;

    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));

    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    ESP_ERROR_CHECK(esp_ble_gap_register_callback(ble_gap_event_handler));
    ESP_ERROR_CHECK(esp_ble_gatts_register_callback(ble_gatt_event_handler));
    ESP_ERROR_CHECK(esp_ble_gatts_app_register(PROFILE_APP_ID));

    return ESP_OK;
}

esp_err_t ble_start_advertising(void) {
    esp_err_t ret = esp_ble_gap_config_adv_data(&adv_data);
    if (ret != ESP_OK) return ret;
    return esp_ble_gap_start_advertising(&adv_params);
}

esp_err_t ble_stop_advertising(void) {
    return esp_ble_gap_stop_advertising();
}

esp_err_t ble_send_data(uint8_t* data, size_t length) {
    if (is_connected != BLE_CONNECTED || !notify_enabled || char_handle == 0) {
        return ESP_FAIL;
    }
    return esp_ble_gatts_send_indicate(gatts_if_handle, conn_id, char_handle, length, data, false);
}

void ble_session_timeout_check(void) {
    if (is_connected == BLE_CONNECTED) {
        uint32_t now = esp_log_timestamp();
        if ((now - current_session.last_activity_time) > BLE_SESSION_TIMEOUT_MS) {
            esp_ble_gap_stop_advertising();
            esp_ble_gatts_close(gatts_if_handle, conn_id);
            is_connected = BLE_DISCONNECTED;
            notify_enabled = false;
        }
    }
}

uint8_t ble_get_connection_status(void) {
    return is_connected;
}

const ble_session_info_t* ble_get_session_info(void) {
    return &current_session;
}

void ble_task(void* param) {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        ble_session_timeout_check();
    }
}
