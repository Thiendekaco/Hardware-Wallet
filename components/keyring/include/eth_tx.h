#ifndef ETH_TX_H
#define ETH_TX_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    uint64_t chain_id;
    uint64_t nonce;
    uint64_t max_priority_fee_per_gas;
    uint64_t max_fee_per_gas;
    uint64_t gas_limit;
    uint8_t to[20];
    bool to_present;
    uint64_t value;
    const uint8_t *data;
    size_t data_len;
} eth_tx_t;

bool eth_tx_decode(const uint8_t *raw, size_t raw_len, eth_tx_t *out_tx);

#endif // ETH_TX_H