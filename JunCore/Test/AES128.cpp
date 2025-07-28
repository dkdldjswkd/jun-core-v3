#include "AES128.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/aes.h>
#include <stdexcept>
#include <thread>

// ============================================================================
// High-Performance AES-128 Implementation
// Thread-local Context 재사용으로 new/free 오버헤드 제거
// ============================================================================

AES128::AESContext::AESContext() {
    ctx_ = EVP_CIPHER_CTX_new();
    if (!ctx_) {
        throw std::runtime_error("Failed to create AES context");
    }
}

AES128::AESContext::~AESContext() {
    if (ctx_) {
        EVP_CIPHER_CTX_free(ctx_);
    }
}

bool AES128::AESContext::EncryptAES128(
    const std::vector<unsigned char>& plaintext,
    const std::vector<unsigned char>& key,
    const std::vector<unsigned char>& iv,
    std::vector<unsigned char>& ciphertext) noexcept
{
    // 게임서버 최적화: 빈번한 호출을 위한 성능 튜닝
    
    // 1. Context 리셋 (이전 암호화 상태 클리어)
    if (EVP_CIPHER_CTX_reset(ctx_) != 1) [[unlikely]] {
        return false;
    }
    
    // 2. AES-128-CBC 초기화 (키+IV 설정)
    if (EVP_EncryptInit_ex(ctx_, EVP_aes_128_cbc(), nullptr, key.data(), iv.data()) != 1) [[unlikely]] {
        return false;
    }
    
    // 3. 출력 버퍼 미리 할당 (재할당 방지)
    const size_t max_output_size = plaintext.size() + AES_BLOCK_SIZE;
    ciphertext.resize(max_output_size);
    
    int update_len = 0;
    int final_len = 0;
    
    // 4. 메인 암호화 (16바이트 블록 단위 처리)
    if (EVP_EncryptUpdate(ctx_, ciphertext.data(), &update_len, 
                         plaintext.data(), static_cast<int>(plaintext.size())) != 1) [[unlikely]] {
        return false;
    }
    
    // 5. 패딩 및 최종 블록 처리
    if (EVP_EncryptFinal_ex(ctx_, ciphertext.data() + update_len, &final_len) != 1) [[unlikely]] {
        return false;
    }
    
    // 6. 정확한 크기로 조정 (메모리 효율)
    ciphertext.resize(update_len + final_len);
    
    return true;
}

bool AES128::AESContext::DecryptAES128(
    const std::vector<unsigned char>& ciphertext,
    const std::vector<unsigned char>& key,
    const std::vector<unsigned char>& iv,
    std::vector<unsigned char>& plaintext) noexcept
{
    // 게임서버 최적화: 빈번한 패킷 복호화를 위한 성능 튜닝
    
    // 1. Context 리셋 및 복호화 초기화
    if (EVP_CIPHER_CTX_reset(ctx_) != 1 ||
        EVP_DecryptInit_ex(ctx_, EVP_aes_128_cbc(), nullptr, key.data(), iv.data()) != 1) [[unlikely]] {
        return false;
    }
    
    // 2. 출력 버퍼 미리 할당 (암호문과 동일 크기)
    plaintext.resize(ciphertext.size());
    
    int update_len = 0;
    int final_len = 0;
    
    // 3. 메인 복호화 (16바이트 블록 단위 처리)
    if (EVP_DecryptUpdate(ctx_, plaintext.data(), &update_len,
                         ciphertext.data(), static_cast<int>(ciphertext.size())) != 1) [[unlikely]] {
        return false;
    }
    
    // 4. 패딩 검증 및 제거 (무결성 검사 포함)
    if (EVP_DecryptFinal_ex(ctx_, plaintext.data() + update_len, &final_len) != 1) [[unlikely]] {
        // 패킷 변조 또는 잘못된 키/IV 감지
        return false;
    }
    
    // 5. 패딩 제거된 원본 크기로 조정
    plaintext.resize(update_len + final_len);
    
    return true;
}

// ============================================================================
// Thread-local Context Pool (고성능 패턴)
// ============================================================================

AES128::AESContext& AES128::GetThreadLocalContext() {
    thread_local AESContext ctx;
    return ctx;
}

// ============================================================================
// Public Interface (간단한 인터페이스)
// ============================================================================

bool AES128::Encrypt(
    const std::vector<unsigned char>& plaintext,
    const std::vector<unsigned char>& key,
    const std::vector<unsigned char>& iv,
    std::vector<unsigned char>& ciphertext) noexcept
{
    return GetThreadLocalContext().EncryptAES128(plaintext, key, iv, ciphertext);
}

bool AES128::Decrypt(
    const std::vector<unsigned char>& ciphertext,
    const std::vector<unsigned char>& key,
    const std::vector<unsigned char>& iv,
    std::vector<unsigned char>& plaintext) noexcept
{
    return GetThreadLocalContext().DecryptAES128(ciphertext, key, iv, plaintext);
}

// ============================================================================
// Utility Functions (랜덤 키/IV 생성)
// ============================================================================

std::vector<unsigned char> AES128::GenerateRandomKey()
{
    // AES-128용 16바이트 안전한 랜덤 키 생성
    std::vector<unsigned char> key(16);
    
    if (RAND_bytes(key.data(), 16) != 1) {
        throw std::runtime_error("Failed to generate AES-128 key");
    }
    
    return key;
}

std::vector<unsigned char> AES128::GenerateRandomIV()
{
    // AES 블록 크기와 동일한 16바이트 IV 생성
    std::vector<unsigned char> iv(AES_BLOCK_SIZE);
    
    if (RAND_bytes(iv.data(), AES_BLOCK_SIZE) != 1) {
        throw std::runtime_error("Failed to generate random IV");
    }
    
    return iv;
}

