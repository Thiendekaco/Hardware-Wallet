#include <curves.h>
#include <secp256k1.h>

#include "bip39.h"
#include "bip32.h"
#include "ecdsa.h"
#include "sha3.h"
#include <string.h>
#include <stdint.h>
#include "u8g2.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static uint8_t g_private_key[32];
static uint8_t g_public_key[65];
static char g_address[43]; // "0x" + 40 hex + '\0'
extern u8g2_t u8g2; // Assume display is globally initialized
static const char *TAG = "mnemonic";

// Path for m/44'/60'/0'/0/0
static const uint32_t ETH_DERIVATION_PATH[] = {
    44 | 0x80000000,
    60 | 0x80000000,
    0  | 0x80000000,
    0,
    0
};
#define ETH_DERIVATION_PATH_LEN 5

void show_message(const char *msg) {
    u8g2_ClearBuffer(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_profont10_tf);
    u8g2_DrawStr(&u8g2, 20, 20, msg);
    u8g2_SendBuffer(&u8g2);
    vTaskDelay(pdMS_TO_TICKS(1500));
}

bool create_account(const char *mnemonic) {
    uint8_t seed[64];
    mnemonic_to_seed(mnemonic, "", seed, NULL);

    HDNode node;
    if (!hdnode_from_seed(seed, 64, SECP256K1_NAME, &node)) {
        show_message("Master key failed!");
        return false;
    }
    if (!hdnode_private_ckd_path(&node, ETH_DERIVATION_PATH, ETH_DERIVATION_PATH_LEN)) {
        show_message("Path derive fail!");
        return false;
    }

    memcpy(g_private_key, node.private_key, 32);
    hdnode_fill_public_key(&node);
    memcpy(g_public_key, node.public_key, 65);

    // Get Ethereum address
    uint8_t pubkey_hash[32];
    keccak_256(node.public_key + 1, 64, pubkey_hash); // skip 0x04 prefix
    sprintf(g_address, "0x");
    for (int i = 12; i < 32; i++) { // Last 20 bytes
        sprintf(g_address + 2 + (i-12)*2, "%02x", pubkey_hash[i]);
    }

    show_message("Account created!");
    return true;
}

bool get_account(char *address, int address_size) {
    if (!g_address[0]) return false;
    strncpy(address, g_address, address_size-1);
    address[address_size-1] = 0;
    return true;
}

bool get_public_key(uint8_t *public_key, int key_size) {
    if (key_size < 65) return false;
    memcpy(public_key, g_public_key, 65);
    return true;
}

bool sign_message(const char *message, uint8_t *signature, int signature_size) {
    if (signature_size < 64) return false;
    uint8_t hash[32];
    keccak_256((const uint8_t *)message, strlen(message), hash);
    int ok = ecdsa_sign(&secp256k1, g_private_key, hash, signature, NULL, NULL);
    return (ok == 0);
}

bool sign_transaction(const uint8_t *tx_data, int tx_len, uint8_t *signature, int signature_size) {
    if (signature_size < 64) return false;
    uint8_t hash[32];
    keccak_256(tx_data, tx_len, hash);
    int ok = ecdsa_sign(&secp256k1, g_private_key, hash, signature, NULL, NULL);
    return (ok == 0);
}
