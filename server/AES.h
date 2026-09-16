#pragma once
#include "GlobalHeaders.h"

#include <iostream>
#include <stdexcept>
#include <iomanip>
#include <cstring>
#include <cstdint>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>



class FastAesGcmProcessor {
private:
    EVP_CIPHER_CTX* enc_ctx;
    EVP_CIPHER_CTX* dec_ctx;
#define KEYSIZE 32
#define IVSIZE 16
#define TAGSIZE 16
#define PADDINGSIZE 8
    static constexpr int AES_256_KEY_SIZE = 32;
    static constexpr int GCM_IV_SIZE = 12;
    static constexpr int GCM_TAG_SIZE = 16;
    int encrypt(const uint8_t *plaintext, int plaintext_len, const uint8_t *key, uint8_t *out_buffer);
    int decrypt(const uint8_t* packed_data, int packed_len, const uint8_t* key, uint8_t* out_plaintext);
public:
    void generate_random_aes_key(char *key);
    FastAesGcmProcessor();
    ~FastAesGcmProcessor();
    FastAesGcmProcessor(const FastAesGcmProcessor&) = delete;
    FastAesGcmProcessor& operator=(const FastAesGcmProcessor&) = delete;

    uint32_t doEncrypt(void *dataPtr, uint32_t dataSize);

    uint32_t doDecrypt(void *dataPtr, uint32_t dataSize);
};