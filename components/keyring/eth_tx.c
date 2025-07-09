#include "eth_tx.h"
#include <string.h>

static uint64_t read_uint_be(const uint8_t *buf, size_t len) {
    uint64_t v = 0;
    for(size_t i=0;i<len;i++) {
        v = (v<<8) | buf[i];
    }
    return v;
}

static bool rlp_decode_item(const uint8_t *buf, size_t buf_len, size_t *pos,
                            const uint8_t **out, size_t *out_len, bool *out_is_list)
{
    if(*pos >= buf_len) return false;
    uint8_t b = buf[*pos];
    if(b <= 0x7f) {
        *out = &buf[*pos];
        *out_len = 1;
        *out_is_list = false;
        (*pos)++;
        return true;
    } else if(b <= 0xb7) {
        size_t len = b - 0x80;
        if(*pos + 1 + len > buf_len) return false;
        *out = &buf[*pos + 1];
        *out_len = len;
        *out_is_list = false;
        *pos += 1 + len;
        return true;
    } else if(b <= 0xbf) {
        size_t len_of_len = b - 0xb7;
        if(*pos + 1 + len_of_len > buf_len) return false;
        size_t len = read_uint_be(&buf[*pos + 1], len_of_len);
        if(*pos + 1 + len_of_len + len > buf_len) return false;
        *out = &buf[*pos + 1 + len_of_len];
        *out_len = len;
        *out_is_list = false;
        *pos += 1 + len_of_len + len;
        return true;
    } else if(b <= 0xf7) {
        size_t len = b - 0xc0;
        if(*pos + 1 + len > buf_len) return false;
        *out = &buf[*pos + 1];
        *out_len = len;
        *out_is_list = true;
        *pos += 1 + len;
        return true;
    } else {
        size_t len_of_len = b - 0xf7;
        if(*pos + 1 + len_of_len > buf_len) return false;
        size_t len = read_uint_be(&buf[*pos + 1], len_of_len);
        if(*pos + 1 + len_of_len + len > buf_len) return false;
        *out = &buf[*pos + 1 + len_of_len];
        *out_len = len;
        *out_is_list = true;
        *pos += 1 + len_of_len + len;
        return true;
    }
}

static bool decode_uint64(const uint8_t *data, size_t len, uint64_t *v) {
    if(len == 0) { *v = 0; return true; }
    if(len > 8) return false;
    *v = read_uint_be(data, len);
    return true;
}

bool eth_tx_decode(const uint8_t *raw, size_t raw_len, eth_tx_t *out_tx) {
    if(!raw || !out_tx || raw_len == 0) return false;
    size_t pos = 0;
    if(raw[pos] != 0x02) return false; // only EIP-1559 supported
    pos++;

    const uint8_t *list_ptr; size_t list_len; bool is_list;
    if(!rlp_decode_item(raw, raw_len, &pos, &list_ptr, &list_len, &is_list) || !is_list) return false;

    size_t lpos = 0;
    const uint8_t *item; size_t ilen; bool ilist;

    // chain id
    if(!rlp_decode_item(list_ptr, list_len, &lpos, &item, &ilen, &ilist) || ilist) return false;
    if(!decode_uint64(item, ilen, &out_tx->chain_id)) return false;

    // nonce
    if(!rlp_decode_item(list_ptr, list_len, &lpos, &item, &ilen, &ilist) || ilist) return false;
    if(!decode_uint64(item, ilen, &out_tx->nonce)) return false;

    // max priority fee per gas
    if(!rlp_decode_item(list_ptr, list_len, &lpos, &item, &ilen, &ilist) || ilist) return false;
    if(!decode_uint64(item, ilen, &out_tx->max_priority_fee_per_gas)) return false;

    // max fee per gas
    if(!rlp_decode_item(list_ptr, list_len, &lpos, &item, &ilen, &ilist) || ilist) return false;
    if(!decode_uint64(item, ilen, &out_tx->max_fee_per_gas)) return false;

    // gas limit
    if(!rlp_decode_item(list_ptr, list_len, &lpos, &item, &ilen, &ilist) || ilist) return false;
    if(!decode_uint64(item, ilen, &out_tx->gas_limit)) return false;

    // to
    if(!rlp_decode_item(list_ptr, list_len, &lpos, &item, &ilen, &ilist) || ilist) return false;
    if(ilen == 0) {
        out_tx->to_present = false;
        memset(out_tx->to, 0, 20);
    } else if(ilen == 20) {
        out_tx->to_present = true;
        memcpy(out_tx->to, item, 20);
    } else {
        return false;
    }

    // value
    if(!rlp_decode_item(list_ptr, list_len, &lpos, &item, &ilen, &ilist) || ilist) return false;
    if(!decode_uint64(item, ilen, &out_tx->value)) return false;

    // data
    if(!rlp_decode_item(list_ptr, list_len, &lpos, &item, &ilen, &ilist) || ilist) return false;
    out_tx->data = item;
    out_tx->data_len = ilen;

    // skip access list
    if(!rlp_decode_item(list_ptr, list_len, &lpos, &item, &ilen, &ilist)) return false;

    return true;
}