#ifndef KEYRING_H
#define KEYRING_H

#include "bip32.h"
#include "secp256k1.h"

// Public variables for storing public key and address
extern uint8_t g_public_key[65];     // Store public key (65 bytes)
extern char g_address[43];           // Store Ethereum address (43 bytes)

// Functions for creating and managing accounts
void create_account_flow();  // Create and store account in NVS
bool get_account_flow(uint32_t account_index, uint8_t *response, size_t *response_size); // Get account with index from NVS
bool get_public_key(HDNode *node, uint8_t *public_key, int key_size);  // Get public key for given HDNode
bool get_address(HDNode *node, char *address, int address_size);  // Get address for given HDNode
bool get_address_raw(HDNode *node, uint8_t *address_raw);

// Signing functions
int sign_message_flow(const char *message, uint8_t *signature, int signature_size, uint32_t account_index);  // Sign message
int sign_transaction_flow(const uint8_t *tx_data, int tx_len, uint8_t *signature, int signature_size, uint32_t account_index);  // Sign transaction
bool isHaveAccount();  // Check if account exists
#endif // KEYRING_H
