#ifndef KEYRING_H
#define KEYRING_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Create a new account using the provided mnemonic phrase.
 * @param mnemonic The BIP39 mnemonic phrase to use for account creation.
 * @return true if account creation is successful, false otherwise.
 */
bool create_account(const char *mnemonic);

/**
 * @brief UI flow for creating an account: handles mnemonic selection/import, then creates account.
 * This function handles the entire user interaction flow and creates the account automatically.
 */
void create_account_flow();

/**
 * @brief Sign a generic message with the current account's private key.
 * @param message The message to sign.
 * @param signature Output buffer for the signature.
 * @param signature_size Size of the signature buffer.
 * @return true if signing is successful, false otherwise.
 */
bool sign_message(const char *message, uint8_t *signature, int signature_size);

/**
 * @brief Sign a transaction with the current account's private key.
 * @param tx_data Raw transaction data to sign.
 * @param tx_len Length of transaction data.
 * @param signature Output buffer for the signature.
 * @param signature_size Size of the signature buffer.
 * @return true if signing is successful, false otherwise.
 */
bool sign_transaction(const uint8_t *tx_data, int tx_len, uint8_t *signature, int signature_size);

/**
 * @brief Get the public key for the current account.
 * @param public_key Output buffer for the public key.
 * @param key_size Size of the output buffer.
 * @return true if successful, false otherwise.
 */
bool get_public_key(uint8_t *public_key, int key_size);

/**
 * @brief Get the account address for the current account.
 * @param address Output buffer for the account address (null-terminated string).
 * @param address_size Size of the output buffer.
 * @param address_index Index of the address to retrieve (0 for first account).
 * @return true if successful, false otherwise.
 */
bool get_account(char *address, int address_size, int address_index);

#endif // KEYRING_H
