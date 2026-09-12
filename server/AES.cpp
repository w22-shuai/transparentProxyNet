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



std::shared_ptr<std::vector<uint8_t>> FastAesGcmProcessor::doEncrypt
(void *dataPtr, long long dataSize) {
    std::shared_ptr<std::vector<uint8_t>> totalData=
        std::make_shared<std::vector<uint8_t>>(KEYSIZE+IVSIZE+dataSize+TAGSIZE+PADDINGSIZE);
    const uint8_t* ptr=totalData->data();
    generate_random_aes_key((char*)ptr);
    long long encryptDataLength=encrypt(( const uint8_t *)dataPtr,dataSize,
        ptr,const_cast<uint8_t*>(ptr+KEYSIZE));
    totalData->resize(encryptDataLength+KEYSIZE);//减少内存,让收发数量上也比较严谨
    return totalData;
}




std::shared_ptr<std::vector<uint8_t>> FastAesGcmProcessor::doDecrypt
(void *dataPtr, long long dataSize)
{
    std::shared_ptr<std::vector<uint8_t>> originData=std::make_shared<std::vector<uint8_t>>(dataSize);
    const uint8_t *key=(const uint8_t *)dataPtr;//初始32位
    long long decryptDataLength=decrypt((( const uint8_t *)dataPtr)+KEYSIZE,
        dataSize-KEYSIZE,key,originData->data());
    if (decryptDataLength<=0) {
        return nullptr;
    }
    originData->resize(decryptDataLength);
    return originData;
};




