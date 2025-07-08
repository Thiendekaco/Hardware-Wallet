#include "keyring.h"
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

// Global variables to store public key and address
extern u8g2_t u8g2;
uint8_t g_public_key[65];    // Store public key
char g_address[43];          // Store Ethereum address

// Path for m/44'/60'/0'/0/0 (Ethereum BIP-44 path)
static const uint32_t ETH_DERIVATION_PATH[] = {
    44 | 0x80000000,
    60 | 0x80000000,
    0  | 0x80000000,
    0,
    0
};
#define ETH_DERIVATION_PATH_LEN 5

// Save HDNode to NVS
void save_hdnode_to_nvs(HDNode *node) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("key_storage", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        show_message("Failed to open NVS storage!");
        return;
    }

    // Save private key
    err = nvs_set_blob(handle, "hdnode_private_key", node->private_key, 32);
    if (err != ESP_OK) {
        show_message("Failed to save private key!");
        return;
    }

    // Save public key
    err = nvs_set_blob(handle, "hdnode_public_key", node->public_key, 65);
    if (err != ESP_OK) {
        show_message("Failed to save public key!");
        return;
    }

    // Save chain code
    err = nvs_set_blob(handle, "hdnode_chain_code", node->chain_code, 32);
    if (err != ESP_OK) {
        show_message("Failed to save chain code!");
        return;
    }

    nvs_commit(handle);
    nvs_close(handle);
    show_message("HDNode saved!");
}

// Load HDNode from NVS
bool load_hdnode_from_nvs(HDNode *node) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("key_storage", NVS_READONLY, &handle);
    if (err != ESP_OK) {
        show_message("Failed to open NVS storage!");
        return false;
    }

    // Load private key
    size_t private_key_size = 32;
    err = nvs_get_blob(handle, "hdnode_private_key", node->private_key, &private_key_size);
    if (err != ESP_OK) {
        show_message("Failed to load private key!");
        return false;
    }

    // Load public key
    size_t public_key_size = 65;
    err = nvs_get_blob(handle, "hdnode_public_key", node->public_key, &public_key_size);
    if (err != ESP_OK) {
        show_message("Failed to load public key!");
        return false;
    }

    // Load chain code
    size_t chain_code_size = 32;
    err = nvs_get_blob(handle, "hdnode_chain_code", node->chain_code, &chain_code_size);
    if (err != ESP_OK) {
        show_message("Failed to load chain code!");
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
    mnemonic_to_seed(mnemonic, "", seed, NULL);
    mnemonic_clear();

    // Tạo HDNode từ seed
    HDNode node;
    if (!hdnode_from_seed(seed, 64, SECP256K1_NAME, &node)) {
        show_message("Master key failed!");
        return;
    }


    // Derive the account using the path
    if (!hdnode_private_ckd_path(&node, ETH_DERIVATION_PATH, sizeof(ETH_DERIVATION_PATH) / sizeof(ETH_DERIVATION_PATH[0]))) {
        show_message("Path derivation failed!");
        return;
    }

    // Lưu HDNode vào NVS
    save_hdnode_to_nvs(&node);

    // Lấy Ethereum address
    uint8_t pubkey_hash[32];
    keccak_256(node.public_key + 1, 64, pubkey_hash);  // Skip 0x04 prefix
    sprintf(g_address, "0x");
    for (int i = 12; i < 32; i++) { // Last 20 bytes
        sprintf(g_address + 2 + (i - 12) * 2, "%02x", pubkey_hash[i]);
    }

    // Lưu public key vào biến toàn cục
    memcpy(g_public_key, node.public_key, 65);

    show_message("Account created!");
}


// Get account from NVS and derive based on index
bool get_account(uint32_t account_index, HDNode *node) {
    if (!load_hdnode_from_nvs(node)) {
        show_message("Failed to load HDNode!");
        return false;
    }

    // Derive the account based on the index
    uint32_t derivation_path[] = {
        44 | 0x80000000,  // 44' for purpose (BIP44)
        60 | 0x80000000,  // 60' for Ethereum
        account_index | 0x80000000,  // account_index' (hardened)
        0,                  // 0 for change
        0                   // 0 for address index
    };

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
    keccak_256(node->public_key + 1, 64, pubkey_hash); // Skip 0x04 prefix
    snprintf(address, address_size, "0x");

    for (int i = 12; i < 32; i++) { // Last 20 bytes
        snprintf(address + 2 + (i - 12) * 2, address_size - (i - 12) * 2, "%02x", pubkey_hash[i]);
    }

    return true;
}

// Get public key from the HDNode
bool get_public_key(HDNode *node, uint8_t *public_key, int key_size) {
    if (key_size < 65) return false;
    memcpy(public_key, node->public_key, 65);
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

    // Get address and public key from the HDNode
    char address[43];
    if (!get_address(&node, address, sizeof(address))) {
        show_message("Failed to get address!");
        return false;  // Failed to get address
    }
    uint8_t public_key[65];
    if (!get_public_key(&node, public_key, sizeof(public_key))) {
        show_message("Failed to get public key!");
        return false;  // Failed to get public key
    }

    // Prepare chainCode (32 bytes)
    uint8_t chainCode[32];
    memcpy(chainCode, node.chain_code, 32);

    // Start constructing the response
    size_t total_size = 1 +  // Public key length
                        1 +  // Address length
                        65 + // Public key length
                        1 +   // Address length
                        32;  // Chain code length


    if (response == NULL) {
        *response_size = total_size;
        return true;
    }

    // Fill the response buffer with the necessary data
    response[0] = 65;  // Length of the public key
    response[1] = 20;  // Length of the address

    // Copy the public key, address, and chain code
    memcpy(response + 2, public_key, 65);  // Public key
    memcpy(response + 2 + 65, address, 20);  // Address
    memcpy(response + 2 + 65 + 20, chainCode, 32);  // Chain code

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




