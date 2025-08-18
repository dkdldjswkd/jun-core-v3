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
    const unsigned char* plaintext, size_t plaintext_size,
    const unsigned char* key,
    const unsigned char* iv,
    unsigned char* ciphertext, size_t max_ciphertext_size) noexcept
{
    // 게임서버 최적화: 빈번한 호출을 위한 성능 튜닝
    
    // 1. 버퍼 크기 검증 (GetEncryptedSize()와 일치해야 함)
    const size_t expected_size = ((plaintext_size + 15) / 16) * 16;
    if (max_ciphertext_size != expected_size) [[unlikely]] {
        return false;
    }
    
    // 2. Context 리셋 (이전 암호화 상태 클리어)
    if (EVP_CIPHER_CTX_reset(ctx_) != 1) [[unlikely]] {
        return false;
    }
    
    // 3. AES-128-CBC 초기화 (키+IV 설정)
    if (EVP_EncryptInit_ex(ctx_, EVP_aes_128_cbc(), nullptr, key, iv) != 1) [[unlikely]] {
        return false;
    }
    
    int update_len = 0;
    int final_len = 0;
    
    // 4. 메인 암호화 (16바이트 블록 단위 처리)
    if (EVP_EncryptUpdate(ctx_, ciphertext, &update_len, 
                         plaintext, static_cast<int>(plaintext_size)) != 1) [[unlikely]] {
        return false;
    }
    
    // 5. 패딩 및 최종 블록 처리
    if (EVP_EncryptFinal_ex(ctx_, ciphertext + update_len, &final_len) != 1) [[unlikely]] {
        return false;
    }
    
    // 6. 암호화 완료 (크기는 GetEncryptedSize()와 동일)
    return true;
}

bool AES128::AESContext::DecryptAES128(
    const unsigned char* ciphertext, size_t ciphertext_size,
    const unsigned char* key,
    const unsigned char* iv,
    unsigned char* plaintext, size_t max_plaintext_size,
    size_t* actual_plaintext_size) noexcept
{
    // 게임서버 최적화: 빈번한 패킷 복호화를 위한 성능 튜닝
    
    // 1. 버퍼 크기 검증
    if (max_plaintext_size < ciphertext_size) [[unlikely]] {
        return false;
    }
    
    // 2. Context 리셋 및 복호화 초기화
    if (EVP_CIPHER_CTX_reset(ctx_) != 1 ||
        EVP_DecryptInit_ex(ctx_, EVP_aes_128_cbc(), nullptr, key, iv) != 1) [[unlikely]] {
        return false;
    }
    
    int update_len = 0;
    int final_len = 0;
    
    // 3. 메인 복호화 (16바이트 블록 단위 처리)
    if (EVP_DecryptUpdate(ctx_, plaintext, &update_len,
                         ciphertext, static_cast<int>(ciphertext_size)) != 1) [[unlikely]] {
        return false;
    }
    
    // 4. 패딩 검증 및 제거 (무결성 검사 포함)
    if (EVP_DecryptFinal_ex(ctx_, plaintext + update_len, &final_len) != 1) [[unlikely]] {
        // 패킷 변조 또는 잘못된 키/IV 감지
        return false;
    }
    
    // 5. 실제 복호화된 크기 반환
    *actual_plaintext_size = update_len + final_len;
    
    return true;
}

bool AES128::AESContext::EncryptAES128_ECB(
    const unsigned char* plaintext, size_t plaintext_size,
    const unsigned char* key,
    unsigned char* ciphertext, size_t max_ciphertext_size) noexcept
{
    // ECB 모드 암호화 (IV 없음)
    
    // 1. 버퍼 크기 검증
    const size_t expected_size = ((plaintext_size + 15) / 16) * 16;
    if (max_ciphertext_size != expected_size) [[unlikely]] {
        return false;
    }
    
    // 2. Context 리셋
    if (EVP_CIPHER_CTX_reset(ctx_) != 1) [[unlikely]] {
        return false;
    }
    
    // 3. AES-128-ECB 초기화 (IV 없음)
    if (EVP_EncryptInit_ex(ctx_, EVP_aes_128_ecb(), nullptr, key, nullptr) != 1) [[unlikely]] {
        return false;
    }
    
    int update_len = 0;
    int final_len = 0;
    
    // 4. 메인 암호화
    if (EVP_EncryptUpdate(ctx_, ciphertext, &update_len, 
                         plaintext, static_cast<int>(plaintext_size)) != 1) [[unlikely]] {
        return false;
    }
    
    // 5. 패딩 및 최종 블록 처리
    if (EVP_EncryptFinal_ex(ctx_, ciphertext + update_len, &final_len) != 1) [[unlikely]] {
        return false;
    }
    
    return true;
}

bool AES128::AESContext::DecryptAES128_ECB(
    const unsigned char* ciphertext, size_t ciphertext_size,
    const unsigned char* key,
    unsigned char* plaintext, size_t max_plaintext_size,
    size_t* actual_plaintext_size) noexcept
{
    // ECB 모드 복호화 (IV 없음)
    
    // 1. 버퍼 크기 검증
    if (max_plaintext_size < ciphertext_size) [[unlikely]] {
        return false;
    }
    
    // 2. Context 리셋 및 복호화 초기화
    if (EVP_CIPHER_CTX_reset(ctx_) != 1 ||
        EVP_DecryptInit_ex(ctx_, EVP_aes_128_ecb(), nullptr, key, nullptr) != 1) [[unlikely]] {
        return false;
    }
    
    int update_len = 0;
    int final_len = 0;
    
    // 3. 메인 복호화
    if (EVP_DecryptUpdate(ctx_, plaintext, &update_len,
                         ciphertext, static_cast<int>(ciphertext_size)) != 1) [[unlikely]] {
        return false;
    }
    
    // 4. 패딩 검증 및 제거
    if (EVP_DecryptFinal_ex(ctx_, plaintext + update_len, &final_len) != 1) [[unlikely]] {
        return false;
    }
    
    // 5. 실제 복호화된 크기 반환
    *actual_plaintext_size = update_len + final_len;
    
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
// Public Interface (포인터 기반 - 고성능)
// ============================================================================

bool AES128::Encrypt(
    const unsigned char* plaintext, size_t plaintext_size,
    const unsigned char* key,
    const unsigned char* iv,
    unsigned char* ciphertext, size_t max_ciphertext_size) noexcept
{
    return GetThreadLocalContext().EncryptAES128(
        plaintext, plaintext_size, key, iv, 
        ciphertext, max_ciphertext_size);
}

bool AES128::Decrypt(
    const unsigned char* ciphertext, size_t ciphertext_size,
    const unsigned char* key,
    const unsigned char* iv,
    unsigned char* plaintext, size_t max_plaintext_size,
    size_t* actual_plaintext_size) noexcept
{
    return GetThreadLocalContext().DecryptAES128(
        ciphertext, ciphertext_size, key, iv,
        plaintext, max_plaintext_size, actual_plaintext_size);
}

// ============================================================================
// Public Interface (vector 기반 - 호환성)
// ============================================================================

bool AES128::Encrypt(
    const std::vector<unsigned char>& plaintext,
    const std::vector<unsigned char>& key,
    const std::vector<unsigned char>& iv,
    std::vector<unsigned char>& ciphertext) noexcept
{
    // GetEncryptedSize로 정확한 크기 할당
    size_t encrypted_size = GetEncryptedSize(plaintext.size());
    ciphertext.resize(encrypted_size);
    
    // 포인터 기반 함수 호출 (크기 조정 불필요)
    return Encrypt(plaintext.data(), plaintext.size(),
                  key.data(), iv.data(),
                  ciphertext.data(), ciphertext.size());
}

bool AES128::Decrypt(
    const std::vector<unsigned char>& ciphertext,
    const std::vector<unsigned char>& key,
    const std::vector<unsigned char>& iv,
    std::vector<unsigned char>& plaintext) noexcept
{
    // 복호화는 최대 암호문과 같은 크기
    plaintext.resize(ciphertext.size());
    
    size_t actual_size;
    bool result = Decrypt(ciphertext.data(), ciphertext.size(),
                         key.data(), iv.data(),
                         plaintext.data(), plaintext.size(),
                         &actual_size);
    
    if (result) {
        plaintext.resize(actual_size); // 패딩 제거된 정확한 크기
    }
    
    return result;
}

// ============================================================================
// Public ECB Interface (IV 없는 암호화)
// ============================================================================

bool AES128::EncryptECB(
    const unsigned char* plaintext, size_t plaintext_size,
    const unsigned char* key,
    unsigned char* ciphertext, size_t max_ciphertext_size) noexcept
{
    return GetThreadLocalContext().EncryptAES128_ECB(
        plaintext, plaintext_size, key, 
        ciphertext, max_ciphertext_size);
}

bool AES128::DecryptECB(
    const unsigned char* ciphertext, size_t ciphertext_size,
    const unsigned char* key,
    unsigned char* plaintext, size_t max_plaintext_size,
    size_t* actual_plaintext_size) noexcept
{
    return GetThreadLocalContext().DecryptAES128_ECB(
        ciphertext, ciphertext_size, key,
        plaintext, max_plaintext_size, actual_plaintext_size);
}

bool AES128::EncryptECB(
    const std::vector<unsigned char>& plaintext,
    const std::vector<unsigned char>& key,
    std::vector<unsigned char>& ciphertext) noexcept
{
    // GetEncryptedSize로 정확한 크기 할당
    size_t encrypted_size = GetEncryptedSize(plaintext.size());
    ciphertext.resize(encrypted_size);
    
    // 포인터 기반 ECB 함수 호출
    return EncryptECB(plaintext.data(), plaintext.size(),
                     key.data(),
                     ciphertext.data(), ciphertext.size());
}

bool AES128::DecryptECB(
    const std::vector<unsigned char>& ciphertext,
    const std::vector<unsigned char>& key,
    std::vector<unsigned char>& plaintext) noexcept
{
    // 복호화는 최대 암호문과 같은 크기
    plaintext.resize(ciphertext.size());
    
    size_t actual_size;
    bool result = DecryptECB(ciphertext.data(), ciphertext.size(),
                            key.data(),
                            plaintext.data(), plaintext.size(),
                            &actual_size);
    
    if (result) {
        plaintext.resize(actual_size); // 패딩 제거된 정확한 크기
    }
    
    return result;
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

