idf_component_register(
        SRCS
        # Core big number, ECDSA, curve for Ethereum
        bignum.c
        ecdsa.c
        curves.c
        secp256k1.c
        nist256p1.c
        rand.c
        hmac.c
        bip32.c
        bip39.c
        pbkdf2.c
        base58.c
        base32.c

        # Ethereum specific
        address.c
        script.c
        sha3.c
        hasher.c
        ripemd160.c
        sha2.c

        # AES encryption (if needed for key storage)
        aes/aescrypt.c
        aes/aeskey.c
        aes/aestab.c
        aes/aes_modes.c

        # Chacha20-Poly1305 (if needed for secure messaging or high-security wallet)
        chacha20poly1305/chacha20poly1305.c
        chacha20poly1305/chacha_merged.c
        chacha20poly1305/poly1305-donna.c
        chacha20poly1305/rfc7539.c

        # Additional hash algorithms
        blake256.c
        blake2b.c
        blake2s.c
        groestl.c
        rc4.c

        # Bech32/SegWit address format support (optional for future expansion)
        segwit_addr.c
        cash_addr.c

        # Utility
        memzero.c
        shamir.c
        hmac_drbg.c
        rfc6979.c
        slip39.c
        INCLUDE_DIRS .
)

add_compile_options(
        -std=gnu99
        -W
        -Wall
        -Wextra
        -Wimplicit-function-declaration
        -Wredundant-decls
        -Wstrict-prototypes
        -Wundef
        -Wshadow
        -Wpointer-arith
        -Wformat
        -Wreturn-type
        -Wsign-compare
        -Wmultichar
        -Wformat-nonliteral
        -Winit-self
        -Wuninitialized
        -Wformat-security
        -Werror
)

target_compile_definitions(${COMPONENT_LIB} PUBLIC
        -DUSE_ETHEREUM=1
        -DUSE_KECCAK=1
        -DUSE_GRAPHENE=0
        -DUSE_MONERO=0
        -DUSE_NEM=0
        -DUSE_CARDANO=0
)
