#include "keyring.h"

#include <esp_log.h>

#include "nvs.h"
#include "nvs_flash.h"
#include "bip39.h"
#include "sha3.h"
#include "ecdsa.h"
#include "secp256k1.h"
#include <string.h>
#include <stdio.h>

#include "curves.h"
#include "memzero.h"
#include "mnemonic.h"
#include "u8g2.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "password.h"
#include "button_listener.h"
#include "ui_state.h"

#define NVS_NAMESPACE  "storage"
#define NVS_PRV_KEY        "private_key"
#define NVS_PUB_KEY        "public_key"
#define NVS_CHAIN_CODE     "chain_code"

// Global variables to store public key and address
extern u8g2_t u8g2;
uint8_t g_public_key[65];    // Store public key
char g_address[43];          // Store Ethereum address

static const char* TAG = "keyring";
// Path for m/44'/60'/0'/0/0 (Ethereum BIP-44 path)
static const uint32_t ETH_DERIVATION_PATH[] = {
    44 | 0x80000000,
    60 | 0x80000000,
    0  | 0x80000000,
    0,
    0
};
#define ETH_DERIVATION_PATH_LEN 5

// Display the address on screen and ask the user to approve sending it.
static bool confirm_send_address(const char *address) {
    ui_wait_until_free();
    ui_set_busy(true);
    int selected = 1; // 0 = No, 1 = Yes

    char line1[22] = {0};
    char line2[22] = {0};
    strncpy(line1, address, 21);
    strncpy(line2, address + 21, 21);

    while (1) {
        u8g2_ClearBuffer(&u8g2);
        u8g2_SetFont(&u8g2, u8g2_font_profont10_tf);
        u8g2_DrawStr(&u8g2, 0, 8, "Share address?");
        u8g2_DrawStr(&u8g2, 0, 16, line1);
        u8g2_DrawStr(&u8g2, 0, 24, line2);
        u8g2_DrawStr(&u8g2, 20, 32, selected ? "[Yes]   No" : " Yes   [No]");
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

// Save HDNode to NVS
void save_hdnode_to_nvs(HDNode *node) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Failed to open NVS storage! %s (%d)", esp_err_to_name(err), err);
        show_message("Failed to open NVS storage!");
        return;
    }

    // Save private key
    err = nvs_set_blob(handle, NVS_PRV_KEY, node->private_key, 32);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Failed to save private key! %s (%d)", esp_err_to_name(err), err);
        show_message("Failed to save private key!");
        return;
    }

    // Save public key (uncompressed)
    err = nvs_set_blob(handle, NVS_PUB_KEY, g_public_key, 65);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Failed to save public key! %s (%d)", esp_err_to_name(err), err);
        show_message("Failed to save public key!");
        return;
    }

    // Save chain code
    err = nvs_set_blob(handle, NVS_CHAIN_CODE, node->chain_code, 32);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Failed to save chain code!");
        show_message("Failed to save chain code!");
        return;
    }

    nvs_commit(handle);
    nvs_close(handle);
    show_message("HDNode saved!");
}


bool isHaveAccount() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        show_message("Failed to open NVS storage!");
        return false;
    }

    // Check if private key exists
    size_t public_key_size = 65;
    err = nvs_get_blob(handle, NVS_PUB_KEY, NULL, &public_key_size);
    nvs_close(handle);

    if (err == ESP_OK && public_key_size == 65) {
        return true; // Account exists
    }

    return false; // No account found
}

// Load HDNode from NVS
bool load_hdnode_from_nvs(HDNode *node) {
    // Initialize structure to a known state before loading data
    memzero(node, sizeof(HDNode));
    node->curve = get_curve_by_name(SECP256K1_NAME);
    node->depth = 0;
    node->child_num = 0;
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        show_message("Failed to open NVS storage!");
        ESP_LOGI(TAG, "Failed to open NVS storage!");
        return false;
    }

    // Load private key
    size_t private_key_size = 32;
    err = nvs_get_blob(handle, NVS_PRV_KEY, node->private_key, &private_key_size);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Failed to load private key!");
        show_message("Failed to load private key!");
        return false;
    }

    // Load public key into temporary buffer
    uint8_t pubkey_buf[65];
    size_t public_key_size = sizeof(pubkey_buf);
    err = nvs_get_blob(handle, NVS_PUB_KEY, pubkey_buf, &public_key_size);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Failed to load public key size: %d", public_key_size);
        show_message("Failed to load public key!");
        return false;
    }
    // Copy to global buffer
    memcpy(g_public_key, pubkey_buf, 65);

    // Load chain code
    size_t chain_code_size = 32;
    err = nvs_get_blob(handle, NVS_CHAIN_CODE, node->chain_code, &chain_code_size);
    if (err != ESP_OK) {
        show_message("Failed to load chain code!");
        ESP_LOGI(TAG, "Failed to load chain code size: %d", chain_code_size);
        return false;
    }

    nvs_close(handle);
    return true;
}

int hdnode_private_ckd_path(HDNode *inout, const uint32_t *path, size_t path_len) {
    for (size_t i = 0; i < path_len; ++i) {
        if (!hdnode_private_ckd(inout, path[i])) {
            return 0;
        }
    }
    return 1;
}


// Create account from mnemonic and save HDNode to NVS
void create_account(const char *mnemonic) {
    uint8_t seed[64];
    ESP_LOGI(TAG, "Creating account from mnemonic: %s", mnemonic);
    mnemonic_to_seed(mnemonic, "", seed, NULL);
    mnemonic_clear();

    show_message("Waiting for seed...");
    // Tạo HDNode từ seed
    HDNode node;
    if (!hdnode_from_seed(seed, 64, SECP256K1_NAME, &node)) {
        show_message("Master key failed!");
        vTaskDelay(pdMS_TO_TICKS(400));
        return;
    }

    show_message("Waiting for path...");
    // Derive the account using the path
    if (!hdnode_private_ckd_path(&node, ETH_DERIVATION_PATH, sizeof(ETH_DERIVATION_PATH) / sizeof(ETH_DERIVATION_PATH[0]))) {
        show_message("Path derivation failed!");
        vTaskDelay(pdMS_TO_TICKS(400));
        return;
    }

    show_message("Waiting for public key...");
    // Compute uncompressed public key and store globally
    ecdsa_get_public_key65(node.curve->params, node.private_key, g_public_key);

    // Save HDNode and public key to NVS
    save_hdnode_to_nvs(&node);

    // Lấy Ethereum address
    uint8_t pubkey_hash[32];
    keccak_256(g_public_key + 1, 64, pubkey_hash);   // Skip 0x04 prefix
    sprintf(g_address, "0x");
    for (int i = 12; i < 32; i++) { // Last 20 bytes
        sprintf(g_address + 2 + (i - 12) * 2, "%02x", pubkey_hash[i]);
    }

    ESP_LOGI(TAG, "Public key: %s", g_address);

    show_message("Account created!");
}


// Get account from NVS and derive based on index
bool get_account(uint32_t account_index, HDNode *node) {
    if (!load_hdnode_from_nvs(node)) {
        show_message("Failed to load HDNode!");
        return false;
    }

    ESP_LOGI(TAG, "Loading account %lu", (unsigned long)account_index);

    // Derive the account based on the index
    uint32_t derivation_path[] = {
        44 | 0x80000000,  // 44' for purpose (BIP44)
        60 | 0x80000000,  // 60' for Ethereum
        account_index | 0x80000000,  // account_index' (hardened)
        0,                  // 0 for change
        0                   // 0 for address index
    };

    ESP_LOGI(TAG, "Deriving account at index %lu", (unsigned long)account_index);

    // Derive the account at the given index
    if (!hdnode_private_ckd_path(node, derivation_path, sizeof(derivation_path) / sizeof(derivation_path[0]))) {
        show_message("Path derivation failed!");
        return false;
    }

    return true;
}


// Get address from the HDNode
bool get_address(HDNode *node, char *address, int address_size) {
    uint8_t pubkey_hash[32];
    keccak_256(g_public_key + 1, 64, pubkey_hash); // Skip 0x04 prefix
    snprintf(address, address_size, "0x");

    for (int i = 12; i < 32; i++) { // Last 20 bytes
        snprintf(address + 2 + (i - 12) * 2, address_size - (i - 12) * 2, "%02x", pubkey_hash[i]);
    }

    return true;
}

// Get raw 20 byte Ethereum address
bool get_address_raw(HDNode *node, uint8_t *address_raw) {
    if (!address_raw) return false;
    hdnode_get_ethereum_pubkeyhash(node, address_raw);
    return true;
}

// Get public key from the HDNode
bool get_public_key(HDNode *node, uint8_t *public_key, int key_size) {
    if (!node || !public_key || key_size < 65) {
        return false;
    }

    // Derive the uncompressed public key directly from the node's private key
    ecdsa_get_public_key65(node->curve->params, node->private_key, public_key);

    return true;
}

// Sign message with the account at the given index
int sign_message(uint32_t accountIndex, const char *message, uint8_t *signature, int signature_size) {
    if (signature_size < 64) return 0;  // Ensure enough space for the signature

    // Derive the account HDNode for the given accountIndex
    HDNode node;
    if (!get_account(accountIndex, &node)) {
        show_message("Failed to get account!");
        return 0;
    }

    // Hash the message using Keccak-256 (Ethereum specific)
    uint8_t hash[32];
    keccak_256((const uint8_t *)message, strlen(message), hash);  // Ethereum uses Keccak-256

    // Sign the hashed message using the derived private key
    int ok = ecdsa_sign(&secp256k1, HASHER_SHA3, node.private_key, hash, sizeof(hash), signature, NULL, NULL);

    // Clear the hash for security
    memzero(hash, sizeof(hash));

    return ok;  // Return true if signing was successful
}


// Sign transaction with the account at the given index
int sign_transaction(uint32_t accountIndex, const uint8_t *tx_data, int tx_len, uint8_t *signature, int signature_size) {
    if (signature_size < 64) {
        return 0;  // Ensure enough space for the signature
    }

    // Derive the account HDNode for the given accountIndex
    HDNode node;
    if (!get_account(accountIndex, &node)) {
        show_message("Failed to get account!");
        return 0;
    }

    // Hash the transaction data using Keccak-256 (Ethereum specific)
    uint8_t hash[32];
    keccak_256(tx_data, tx_len, hash);  // Ethereum uses Keccak-256 hash for transactions

    // Sign the hashed transaction using the derived private key
    int ok = ecdsa_sign(&secp256k1, HASHER_SHA3, node.private_key, hash, sizeof(hash), signature, NULL, NULL);

    // Clear the hash for security
    memzero(hash, sizeof(hash));

    return ok;  // Return true if signing was successful
}

void create_account_flow() {
    char mnemonic[256];

    // Start mnemonic UI flow
    handle_mnemonic_flow(mnemonic);

    // Derive HDNode from mnemonic and create the Ethereum account
    create_account(mnemonic);
}

bool get_account_flow(uint32_t account_index, uint8_t *response, size_t *response_size) {
    // Verify password before retrieving account
    HDNode node;
    if (!get_account(account_index, &node)) {
        return false;  // Failed to get account
    }

    ESP_LOGI(TAG, "Account %lu derived successfully", (unsigned long)account_index);

    // Get address and public key from the HDNode
    uint8_t address_raw[20];
    char address_str[43];
    if (!get_address_raw(&node, address_raw) ||
        !get_address(&node, address_str, sizeof(address_str))) {
        show_message("Failed to get address!");
        return false;  // Failed to get address
    }

    if (!confirm_send_address(address_str)) {
        return false;  // User rejected sharing the address
    }

    ESP_LOGI(TAG, "Loaded address raw");

    uint8_t public_key[65];
    if (!get_public_key(&node, public_key, sizeof(public_key))) {
        show_message("Failed to get public key!");
        return false;  // Failed to get public key
    }

    ESP_LOGI(TAG, "Loaded publickey raw");
    // Prepare chainCode (32 bytes)
    uint8_t chainCode[32];
    memcpy(chainCode, node.chain_code, 32);

    // Start constructing the response
    size_t total_size = 1 +  // Public key length
                        1 +  // Address length
                        65 + // Public key bytes
                        20 + // Address bytes
                        32;  // Chain code length


    ESP_LOGI(TAG, "Total response size: %zu bytes", total_size);
    if (response == NULL) {
        *response_size = total_size;
        return true;
    }

    // Fill the response buffer with the necessary data
    response[0] = 65;  // Length of the public key
    response[1] = 20;  // Length of the address

    // Copy the public key, address, and chain code
    memcpy(response + 2, public_key, 65);         // Public key
    memcpy(response + 2 + 65, address_raw, 20);   // Address
    memcpy(response + 2 + 65 + 20, chainCode, 32); // Chain code

    *response_size = total_size;

    return true;
}

int sign_message_flow(const char *message, uint8_t *signature, int signature_size, uint32_t account_index) {
    // Verify password before signing
    if (!handle_password_flow()) {
        return 0; // Password verification failed
    }

    // Now derive the account based on account_index and sign the message
    return sign_message(account_index, message, signature, signature_size);
}


int sign_transaction_flow(const uint8_t *tx_data, int tx_len, uint8_t *signature, int signature_size, uint32_t account_index) {
    // Verify password before signing
    if (!handle_password_flow()) {
        return 0; // Password verification failed
    }

    // Now derive the account based on account_index and sign the transaction
    return sign_transaction(account_index, tx_data, tx_len, signature, signature_size);
}




