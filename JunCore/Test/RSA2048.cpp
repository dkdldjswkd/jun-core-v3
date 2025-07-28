#include "RSA2048.h"
#include <iostream>
#include <stdexcept>

bool RSA2048::openssl_initialized_ = false;

RSA2048::RSA2048() : rsa_keypair_(nullptr), rsa_public_only_(nullptr)
{
    InitializeOpenSSL();
}

RSA2048::~RSA2048()
{
    if (rsa_keypair_) {
        RSA_free(rsa_keypair_);
        rsa_keypair_ = nullptr;
    }
    if (rsa_public_only_) {
        RSA_free(rsa_public_only_);
        rsa_public_only_ = nullptr;
    }
}

void RSA2048::InitializeOpenSSL()
{
    if (!openssl_initialized_) {
        // OpenSSL 3.0에서는 자동 초기화되므로 별도 초기화 불필요
        // 하지만 랜덤 시드는 명시적으로 설정
        if (RAND_status() != 1) {
            // 시스템에서 랜덤 시드 로드
            RAND_poll();
        }
        openssl_initialized_ = true;
    }
}

bool RSA2048::GenerateKeyPair()
{
    // 기존 키가 있으면 해제
    if (rsa_keypair_) {
        RSA_free(rsa_keypair_);
        rsa_keypair_ = nullptr;
    }

    // RSA 키 생성 컨텍스트 생성
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    if (!ctx) {
        return false;
    }

    // 키 생성 초기화
    if (EVP_PKEY_keygen_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return false;
    }

    // 키 크기 설정 (2048비트)
    if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, RSAConstants::KEY_SIZE_BITS) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return false;
    }

    // 공개 지수 설정 (65537)
    BIGNUM* e = BN_new();
    if (!e || !BN_set_word(e, RSAConstants::PUBLIC_EXPONENT)) {
        BN_free(e);
        EVP_PKEY_CTX_free(ctx);
        return false;
    }

    if (EVP_PKEY_CTX_set1_rsa_keygen_pubexp(ctx, e) <= 0) {
        BN_free(e);
        EVP_PKEY_CTX_free(ctx);
        return false;
    }

    BN_free(e);

    // 키 쌍 생성
    EVP_PKEY* pkey = nullptr;
    if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return false;
    }

    // EVP_PKEY에서 RSA 구조체 추출
    rsa_keypair_ = EVP_PKEY_get1_RSA(pkey);
    
    EVP_PKEY_free(pkey);
    EVP_PKEY_CTX_free(ctx);

    return (rsa_keypair_ != nullptr);
}

std::vector<unsigned char> RSA2048::ExportPublicKey() const
{
    if (!rsa_keypair_) {
        throw std::runtime_error("No key pair available for export");
    }

    // DER 형식으로 공개키 직렬화
    unsigned char* der_buf = nullptr;
    int der_len = i2d_RSA_PUBKEY(rsa_keypair_, &der_buf);
    
    if (der_len <= 0 || !der_buf) {
        throw std::runtime_error("Failed to export public key to DER format");
    }

    // 벡터로 복사
    std::vector<unsigned char> result(der_buf, der_buf + der_len);
    
    // OpenSSL이 할당한 메모리 해제
    OPENSSL_free(der_buf);
    
    return result;
}

bool RSA2048::ImportPublicKey(const std::vector<unsigned char>& publicKeyData)
{
    // 기존 공개키가 있으면 해제
    if (rsa_public_only_) {
        RSA_free(rsa_public_only_);
        rsa_public_only_ = nullptr;
    }

    // DER 형식에서 공개키 로드
    const unsigned char* der_ptr = publicKeyData.data();
    rsa_public_only_ = d2i_RSA_PUBKEY(nullptr, &der_ptr, publicKeyData.size());

    return (rsa_public_only_ != nullptr);
}

bool RSA2048::EncryptWithPublicKey(const std::vector<unsigned char>& plaintext,
                                  std::vector<unsigned char>& ciphertext) const
{
    if (!rsa_public_only_) {
        return false;
    }

    if (plaintext.size() > RSAConstants::MAX_ENCRYPT_SIZE) {
        return false; // 데이터가 너무 큼
    }

    // 암호문 버퍼 준비
    ciphertext.resize(RSAConstants::KEY_SIZE_BYTES);

    // RSA 공개키 암호화 (PKCS#1 v1.5 패딩)
    int encrypted_len = RSA_public_encrypt(
        static_cast<int>(plaintext.size()),
        plaintext.data(),
        ciphertext.data(),
        rsa_public_only_,
        RSA_PKCS1_PADDING
    );

    if (encrypted_len == -1) {
        ciphertext.clear();
        return false;
    }

    // 실제 암호화된 길이로 조정
    ciphertext.resize(encrypted_len);
    return true;
}

bool RSA2048::DecryptWithPrivateKey(const std::vector<unsigned char>& ciphertext,
                                   std::vector<unsigned char>& plaintext) const
{
    if (!rsa_keypair_) {
        return false;
    }

    // 복호문 버퍼 준비 (최대 크기)
    plaintext.resize(RSAConstants::KEY_SIZE_BYTES);

    // RSA 개인키 복호화
    int decrypted_len = RSA_private_decrypt(
        static_cast<int>(ciphertext.size()),
        ciphertext.data(),
        plaintext.data(),
        rsa_keypair_,
        RSA_PKCS1_PADDING
    );

    if (decrypted_len == -1) {
        plaintext.clear();
        return false;
    }

    // 실제 복호화된 길이로 조정
    plaintext.resize(decrypted_len);
    return true;
}

bool RSA2048::HasKeyPair() const
{
    return (rsa_keypair_ != nullptr);
}

bool RSA2048::HasPublicKeyOnly() const
{
    return (rsa_public_only_ != nullptr);
}

std::string RSA2048::GetLastError()
{
    unsigned long error_code = ERR_get_error();
    if (error_code == 0) {
        return "No error";
    }

    char error_buf[256];
    ERR_error_string_n(error_code, error_buf, sizeof(error_buf));
    return std::string(error_buf);
}