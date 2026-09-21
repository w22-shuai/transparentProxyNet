#include "AES.h"




int FastAesGcmProcessor::encrypt(const uint8_t *plaintext, int plaintext_len, const uint8_t *key, uint8_t *out_buffer)  {
    // 1. 生成随机 IV，直接写入输出缓冲区的头部，零拷贝
    uint8_t* iv_ptr = out_buffer;
    if (RAND_bytes(iv_ptr, GCM_IV_SIZE) != 1) {
        return -1; // 随机数生成失败
    }

    if (1 != EVP_EncryptInit_ex(enc_ctx, EVP_aes_256_gcm(), nullptr, key, iv_ptr)) return -1;

    int len = 0;
    int ciphertext_len = 0;
    uint8_t* ciphertext_ptr = out_buffer + GCM_IV_SIZE;

    if (1 != EVP_EncryptUpdate(enc_ctx, ciphertext_ptr, &len, plaintext, plaintext_len)) return -1;
    ciphertext_len = len;

    // 4. 结束加密
    if (1 != EVP_EncryptFinal_ex(enc_ctx, ciphertext_ptr + len, &len)) return -1;
    ciphertext_len += len;

    // 5. 获取 Tag，直接追加到密文尾部
    uint8_t* tag_ptr = ciphertext_ptr + ciphertext_len;
    if (1 != EVP_CIPHER_CTX_ctrl(enc_ctx, EVP_CTRL_GCM_GET_TAG, GCM_TAG_SIZE, tag_ptr)) return -1;

    return GCM_IV_SIZE + ciphertext_len + GCM_TAG_SIZE;
}


void FastAesGcmProcessor::generate_random_aes_key(char*key) {
    if (RAND_bytes(reinterpret_cast<unsigned char*>(key),32) != 1) {
        throw std::runtime_error("Failed to generate random AES key");
    }
}

FastAesGcmProcessor::FastAesGcmProcessor()  {
    enc_ctx = EVP_CIPHER_CTX_new();
    dec_ctx = EVP_CIPHER_CTX_new();
    if (!enc_ctx || !dec_ctx) {
        throw std::runtime_error("Failed to create EVP_CIPHER_CTX");
    }
}

FastAesGcmProcessor::~FastAesGcmProcessor() {
    if (enc_ctx) EVP_CIPHER_CTX_free(enc_ctx);
    if (dec_ctx) EVP_CIPHER_CTX_free(dec_ctx);
}

int FastAesGcmProcessor::decrypt(const uint8_t *packed_data, int packed_len, const uint8_t *key,uint8_t *out_plaintext)  {
    // 基础长度校验
    if (packed_len < (GCM_IV_SIZE + GCM_TAG_SIZE)) return -1;

    // 解析指针 (纯偏移计算，无任何内存分配)
    const uint8_t* iv_ptr = packed_data;
    const uint8_t* ciphertext_ptr = packed_data + GCM_IV_SIZE;
    int ciphertext_len = packed_len - GCM_IV_SIZE - GCM_TAG_SIZE;
    const uint8_t* tag_ptr = ciphertext_ptr + ciphertext_len;

    // 初始化解密 (Context 复用)
    if (1 != EVP_DecryptInit_ex(dec_ctx, EVP_aes_256_gcm(), nullptr, key, iv_ptr)) return -1;

    int len = 0;
    int plaintext_len = 0;

    // 解密数据，直接写入提供的明文缓冲区
    if (1 != EVP_DecryptUpdate(dec_ctx, out_plaintext, &len, ciphertext_ptr, ciphertext_len)) return -1;
    plaintext_len = len;

    // 设置期望的 Tag (去 const 强转是 OpenSSL API 的历史遗留要求)
    if (1 != EVP_CIPHER_CTX_ctrl(dec_ctx, EVP_CTRL_GCM_SET_TAG, GCM_TAG_SIZE, const_cast<uint8_t*>(tag_ptr))) return -1;

    // 校验 Tag！如果失败返回 <=0
    if (EVP_DecryptFinal_ex(dec_ctx, out_plaintext + len, &len) <= 0) {
        return -1; // 触发防篡改机制，立刻丢弃
    }
    plaintext_len += len;

    return plaintext_len;
}


// ========================================================================
// 内存布局约定 (doEncrypt 输出 / doDecrypt 输入):
//
//   [ key(32) | iv(12) | ciphertext(dataSize) | tag(16) | padding(8) ]
//     偏移0       偏移32    偏移44                偏移 44+dataSize        偏移 60+dataSize
//
//   总长度 = 32 + 12 + dataSize + 16 + 8 = dataSize + 68
//
// 调用 doEncrypt 前，调用方需要把明文提前摆放在 dataPtr + KEYSIZE + GCM_IV_SIZE
// (即偏移 44) 处，并保证缓冲区总大小 >= dataSize + 68
// (前面 44 字节留给 key+iv，明文之后还要留 tag(16)+padding(8)=24 字节)。
// 加密是原地(in==out)进行的：密文直接覆盖明文所在的那段内存，不分配新缓冲区。
// ========================================================================

uint32_t FastAesGcmProcessor::doEncrypt(void *dataPtr, uint32_t dataSize) {
    if (!dataPtr || dataSize <= 0) return -1;

    uint8_t* base        = static_cast<uint8_t*>(dataPtr);
    uint8_t* key_ptr      = base;                          // 0，密钥直接写在缓冲区最前面
    uint8_t* enc_out_ptr  = key_ptr + KEYSIZE;              // 32，IV+密文+tag 从这里开始
    uint8_t* plain_ptr    = enc_out_ptr + GCM_IV_SIZE;      // 32+12=44，明文必须已摆放在这里

    generate_random_aes_key(reinterpret_cast<char*>(key_ptr));

    int encLen = encrypt(plain_ptr, static_cast<int>(dataSize), key_ptr, enc_out_ptr);
    if (encLen < 0) {
        return -1;
    }

    // padding 追加在 tag 之后，这里默认清零；如果你想在这 8 字节里塞别的东西
    // (比如校验位、序号)，把下面这行删掉即可，长度计算不受影响。
    uint8_t* padding_ptr = enc_out_ptr + encLen;            // 32 + encLen
    memset(padding_ptr, 0, PADDINGSIZE);

    return static_cast<uint32_t>(KEYSIZE) + static_cast<uint32_t>(encLen) + static_cast<uint32_t>(PADDINGSIZE);
}



uint32_t FastAesGcmProcessor::doDecrypt(void *dataPtr, uint32_t dataSize) {
    const uint32_t minLen = static_cast<uint32_t>(KEYSIZE) + GCM_IV_SIZE + GCM_TAG_SIZE + PADDINGSIZE;
    if (!dataPtr || dataSize < minLen) return -1;

    uint8_t* base       = static_cast<uint8_t*>(dataPtr);
    uint8_t* key_ptr     = base;                            // 0
    uint8_t* packed_ptr  = key_ptr + KEYSIZE;               // 32，IV+密文+tag 连续存放
    int packed_len = static_cast<int>(dataSize - KEYSIZE - PADDINGSIZE);

    // out_plaintext 传 packed_ptr+GCM_IV_SIZE，与内部计算出的 ciphertext_ptr 是同一地址，
    // 同样是 EVP 支持的原地(in==out)解密，明文直接覆盖密文，不需要额外缓冲区。
    int plainLen = decrypt(packed_ptr, packed_len, key_ptr, packed_ptr + GCM_IV_SIZE);
    if (plainLen < 0) {
        return -1;
    }
    return plainLen;
}