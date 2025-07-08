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
#include <stdbool.h>
#include "u8g2.h"
#include "ui_state.h"
#include "button_listener.h"
#include "keyring.h"

static const char* TAG = "BLE_Component";
extern u8g2_t u8g2;
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
static ble_pair_request_t pending_pair_request;
static QueueHandle_t pair_request_queue;

#define BLE_BOND_NAMESPACE "ble_bond"
#define BLE_BOND_KEY "addr"
static esp_bd_addr_t bonded_addr = {0};
static bool has_bonded_addr = false;


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

static bool load_bonded_addr(esp_bd_addr_t addr) {
    nvs_handle_t handle;
    size_t size = sizeof(esp_bd_addr_t);
    esp_err_t err = nvs_open(BLE_BOND_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) return false;
    err = nvs_get_blob(handle, BLE_BOND_KEY, addr, &size);
    nvs_close(handle);
    return err == ESP_OK && size == sizeof(esp_bd_addr_t);
}

static void save_bonded_addr(const esp_bd_addr_t addr) {
    nvs_handle_t handle;
    if (nvs_open(BLE_BOND_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_blob(handle, BLE_BOND_KEY, addr, sizeof(esp_bd_addr_t));
        nvs_commit(handle);
        nvs_close(handle);
    }
}

static bool is_bonded_addr(const esp_bd_addr_t addr) {
    return has_bonded_addr && memcmp(addr, bonded_addr, sizeof(esp_bd_addr_t)) == 0;
}

// Permissions and properties
#define CHAR_PERM (ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE)
#define CHAR_PROP (ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY)


/*
 * BLE command format written to the main characteristic:
 *   byte 0  : command ID
 *   byte 1..n : parameters depending on the command
 *
 * Command IDs
 *   0x01 - Get account information. Parameters: 4 byte big-endian account index.
 *           Response contains public key, address and chain code.
 *   0x02 - Sign message. Parameters: 4 byte account index followed by the
 *           message bytes. The device returns a 64 byte signature.
 *   0x03 - Sign transaction. Parameters: 4 byte account index followed by raw
 *           transaction data. The response is a 64 byte signature.
 */
static void process_ble_command(const uint8_t *data, size_t len) {
    if (len < 1) {
        ESP_LOGE(TAG, "BLE cmd too short");
        return;
    }

    uint8_t cmd = data[0];
    switch (cmd) {
        case 0x01: { // Get account information
            ESP_LOGI(TAG, "Connect account");
            if (len < 5) {
                ESP_LOGE(TAG, "Get account cmd invalid length");
                return;
            }
            uint32_t index = (data[1] << 24) | (data[2] << 16) | (data[3] << 8) | data[4];
            uint8_t resp[128];
            size_t resp_size = 0;

            ESP_LOGI(TAG, "Get account info for index %lu", (unsigned long)index);
            if (get_account_flow(index, resp, &resp_size)) {
                ble_send_data(resp, resp_size);
            } else {
                uint8_t err = 0xff;
                ble_send_data(&err, 1);
            }
            break;
        }

        case 0x02: { // Sign message
            if (len < 5) {
                ESP_LOGE(TAG, "Sign message cmd invalid length");
                return;
            }
            uint32_t index = (data[1] << 24) | (data[2] << 16) | (data[3] << 8) | data[4];
            const size_t msg_len = len - 5;
            char msg_buf[RECV_BUFFER_SIZE];
            if (msg_len >= sizeof(msg_buf)) {
                ESP_LOGE(TAG, "Message too long");
                return;
            }
            memcpy(msg_buf, &data[5], msg_len);
            msg_buf[msg_len] = '\0';

            uint8_t sig[64];
            int ok = sign_message_flow(msg_buf, sig, sizeof(sig), index);
            if (ok) {
                ble_send_data(sig, sizeof(sig));
            } else {
                uint8_t err = 0xfe;
                ble_send_data(&err, 1);
            }
            break;
        }

        case 0x03: { // Sign transaction
            if (len < 5) {
                ESP_LOGE(TAG, "Sign tx cmd invalid length");
                return;
            }
            uint32_t index = (data[1] << 24) | (data[2] << 16) | (data[3] << 8) | data[4];
            const uint8_t *tx = &data[5];
            size_t tx_len = len - 5;
            uint8_t sig[64];
            int ok = sign_transaction_flow(tx, tx_len, sig, sizeof(sig), index);
            if (ok) {
                ble_send_data(sig, sizeof(sig));
            } else {
                uint8_t err = 0xfd;
                ble_send_data(&err, 1);
            }
            break;
        }

        default:
            ESP_LOGW(TAG, "Unknown BLE cmd %02x", cmd);
            break;
    }
}

static bool ble_pairing_flow(const char *code) {
    ui_wait_until_free();
    ui_set_busy(true);
    int selected = 1; // 0 = No, 1 = Yes
    while (1) {
        u8g2_ClearBuffer(&u8g2);
        u8g2_SetFont(&u8g2, u8g2_font_profont10_tf);
        u8g2_DrawStr(&u8g2, 20, 10, "Pairing request");
        u8g2_DrawStr(&u8g2, 40, 20, code);
        u8g2_DrawStr(&u8g2, 20, 30, selected ? "[Yes]   No" : " Yes   [No]");
        u8g2_SendBuffer(&u8g2);

        if (is_button_left_pressed() || is_button_right_pressed()) {
            selected = !selected;
            vTaskDelay(pdMS_TO_TICKS(300));
        }
        if (is_button_middle_pressed()) {
            vTaskDelay(pdMS_TO_TICKS(300));
            ui_set_busy(false);
            return selected == 1;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

static void ble_gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    int dummy = 0;
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
        case ESP_GAP_BLE_SEC_REQ_EVT:
            esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
             break;

        case ESP_GAP_BLE_PASSKEY_NOTIF_EVT:
        case ESP_GAP_BLE_NC_REQ_EVT:
            snprintf(pending_pair_request.code, sizeof(pending_pair_request.code), "%06u", (unsigned int)param->ble_security.key_notif.passkey);
            memcpy(pending_pair_request.remote_bda, param->ble_security.key_notif.bd_addr, sizeof(esp_bd_addr_t));
            xQueueSend(pair_request_queue, &dummy, 0);
            break;

        case ESP_GAP_BLE_AUTH_CMPL_EVT:
            if (param->ble_security.auth_cmpl.success) {
                memcpy(bonded_addr, param->ble_security.auth_cmpl.bd_addr, sizeof(esp_bd_addr_t));
                save_bonded_addr(bonded_addr);
                has_bonded_addr = true;
                esp_ble_set_encryption(param->ble_security.auth_cmpl.bd_addr, ESP_BLE_SEC_ENCRYPT);
            } else {
                ESP_LOGE(TAG, "Auth failed: %d", param->ble_security.auth_cmpl.fail_reason);
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

            ret = esp_ble_gatts_start_service(service_handle);
            if (ret == ESP_OK) {
                esp_ble_gap_config_adv_data(&adv_data);
                esp_ble_gap_start_advertising(&adv_params);
            } else {
                ESP_LOGE(TAG, "Start service failed %d", ret);
            }
            break;
        }

        case ESP_GATTS_CONNECT_EVT: {
            is_connected = BLE_CONNECTED;
            conn_id = param->connect.conn_id;

            for (int i = 0; i < 16; i++) {
                current_session.session_id[i] = esp_random() & 0xFF;
            }
            current_session.last_activity_time = esp_log_timestamp();

            memcpy(pending_pair_request.remote_bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
            if (is_bonded_addr(param->connect.remote_bda)) {
                esp_ble_set_encryption(param->connect.remote_bda, ESP_BLE_SEC_ENCRYPT);
            } else {
                esp_ble_set_encryption(param->connect.remote_bda, ESP_BLE_SEC_ENCRYPT_MITM);
            }
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

                    process_ble_command(recv_buffer, recv_len);

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
    has_bonded_addr = load_bonded_addr(bonded_addr);

    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_MITM_BOND;
    esp_ble_io_cap_t iocap = ESP_IO_CAP_OUT;
    uint8_t key_size = 16;
    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;

    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));

    ESP_ERROR_CHECK(esp_ble_gap_register_callback(ble_gap_event_handler));
    ESP_ERROR_CHECK(esp_ble_gatts_register_callback(ble_gatt_event_handler));
    ESP_ERROR_CHECK(esp_ble_gatts_app_register(PROFILE_APP_ID));

    pair_request_queue = xQueueCreate(1, sizeof(int));

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
    int dummy;

    while (1) {
        if (xQueueReceive(pair_request_queue, &dummy, pdMS_TO_TICKS(500))) {
            bool ok = ble_pairing_flow(pending_pair_request.code);
            esp_ble_confirm_reply(pending_pair_request.remote_bda, ok);
            if (!ok) {
                esp_ble_gap_disconnect(pending_pair_request.remote_bda);
            }
        }
        ble_session_timeout_check();
    }
}

static bool ble_confirm_disconnect(void) {
    ui_wait_until_free();
    ui_set_busy(true);
    int selected = 1; // 0 = No, 1 = Yes
    while (1) {
        u8g2_ClearBuffer(&u8g2);
        u8g2_SetFont(&u8g2, u8g2_font_profont10_tf);
        u8g2_DrawStr(&u8g2, 10, 15, "Disconnect?");
        u8g2_DrawStr(&u8g2, 20, 30, selected ? "[Yes]   No" : " Yes   [No]");
        u8g2_SendBuffer(&u8g2);

        if (is_button_left_pressed() || is_button_right_pressed()) {
            selected = !selected;
            vTaskDelay(pdMS_TO_TICKS(300));
        }
        if (is_button_middle_pressed()) {
            vTaskDelay(pdMS_TO_TICKS(300));
            ui_set_busy(false);
            return selected == 1;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void ble_status_flow(void) {
    ui_wait_until_free();
    if (ble_get_connection_status() == BLE_DISCONNECTED) {
        u8g2_ClearBuffer(&u8g2);
        u8g2_SetFont(&u8g2, u8g2_font_profont10_tf);
        u8g2_DrawStr(&u8g2, 10, 15, "Connect Bluetooth");
        u8g2_DrawStr(&u8g2, 10, 25, "to get account");
        u8g2_SendBuffer(&u8g2);
        return;
    }

    u8g2_ClearBuffer(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_profont10_tf);
    u8g2_DrawStr(&u8g2, 40, 12, "Passkey");
    if (strlen(pending_pair_request.code)) {
        u8g2_DrawStr(&u8g2, 48, 24, pending_pair_request.code);
    } else {
        u8g2_DrawStr(&u8g2, 30, 24, "Connected");
    }
    u8g2_SendBuffer(&u8g2);

    if (is_button_right_pressed()) {
        vTaskDelay(pdMS_TO_TICKS(300));
        if (ble_confirm_disconnect()) {
            esp_ble_gap_disconnect(pending_pair_request.remote_bda);
        }
    }
}