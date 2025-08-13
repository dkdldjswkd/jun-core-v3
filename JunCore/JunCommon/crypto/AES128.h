#pragma once
#include <vector>
#include <openssl/evp.h>

/**
 * @brief 고성능 AES-128 암호화 클래스
 * 
 * - Thread-local Context 재사용으로 new/free 오버헤드 제거
 * - 빈번한 암호화/복호화 작업에 최적화
 * - 메모리 할당 최소화 및 브랜치 예측 최적화
 */
class AES128
{
public:
    /**
     * @brief 암호화 후 예상 크기 반환 (PKCS#7 패딩 포함)
     * @param plaintext_size 원본 데이터 크기
     * @return 암호화 후 예상 크기 (항상 16바이트 배수)
     * 
     * @note PKCS#7 패딩은 항상 1~16바이트 추가됨
     */
    static size_t GetEncryptedSize(size_t plaintext_size) noexcept
    {
        return ((plaintext_size + 15) / 16) * 16;
    }

    /**
     * @brief 데이터 암호화 (포인터 기반 - 고성능)
     * @param plaintext 원본 데이터 포인터
     * @param plaintext_size 원본 데이터 크기
     * @param key AES-128 키 (16바이트)
     * @param iv 초기화 벡터 (16바이트)
     * @param ciphertext 암호화된 데이터 출력 버퍼
     * @param max_ciphertext_size 출력 버퍼 크기
     * @return 성공 시 true, 실패 시 false
     * 
     * @note GetEncryptedSize()로 필요한 버퍼 크기를 정확히 계산 후 호출
     * @note max_ciphertext_size는 반드시 GetEncryptedSize(plaintext_size)와 일치해야 함
     * @note 암호화된 실제 크기는 GetEncryptedSize() 반환값과 동일함
     */
    static bool Encrypt(const unsigned char* plaintext, size_t plaintext_size,
                       const unsigned char* key,
                       const unsigned char* iv,
                       unsigned char* ciphertext, size_t max_ciphertext_size) noexcept;

    /**
     * @brief 데이터 암호화 (vector 기반 - 호환성)
     * @param plaintext 원본 데이터
     * @param key AES-128 키 (16바이트)
     * @param iv 초기화 벡터 (16바이트)
     * @param ciphertext 암호화된 데이터 (출력)
     * @return 성공 시 true, 실패 시 false
     * 
     * @note 기존 코드 호환성을 위한 래퍼 함수
     * @note 성능이 중요한 곳에서는 포인터 기반 버전 사용 권장
     */
    static bool Encrypt(const std::vector<unsigned char>& plaintext,
                       const std::vector<unsigned char>& key,
                       const std::vector<unsigned char>& iv,
                       std::vector<unsigned char>& ciphertext) noexcept;
    
    /**
     * @brief 데이터 복호화 (포인터 기반 - 고성능)
     * @param ciphertext 암호화된 데이터 포인터
     * @param ciphertext_size 암호화된 데이터 크기
     * @param key AES-128 키 (16바이트)
     * @param iv 초기화 벡터 (16바이트)
     * @param plaintext 복호화된 데이터 출력 버퍼
     * @param max_plaintext_size 출력 버퍼 최대 크기
     * @param actual_plaintext_size 실제 복호화된 데이터 크기 (출력)
     * @return 성공 시 true, 실패 시 false (데이터 변조 시 실패)
     * 
     * @note PKCS#7 패딩은 자동으로 제거됨
     * @note max_plaintext_size는 최소 ciphertext_size 이상 권장
     */
    static bool Decrypt(const unsigned char* ciphertext, size_t ciphertext_size,
                       const unsigned char* key,
                       const unsigned char* iv,
                       unsigned char* plaintext, size_t max_plaintext_size,
                       size_t* actual_plaintext_size) noexcept;

    /**
     * @brief 데이터 복호화 (vector 기반 - 호환성)
     * @param ciphertext 암호화된 데이터
     * @param key AES-128 키 (16바이트)
     * @param iv 초기화 벡터 (16바이트)
     * @param plaintext 복호화된 데이터 (출력)
     * @return 성공 시 true, 실패 시 false (데이터 변조 시 실패)
     * 
     * @note 기존 코드 호환성을 위한 래퍼 함수
     * @note 성능이 중요한 곳에서는 포인터 기반 버전 사용 권장
     */
    static bool Decrypt(const std::vector<unsigned char>& ciphertext,
                       const std::vector<unsigned char>& key,
                       const std::vector<unsigned char>& iv,
                       std::vector<unsigned char>& plaintext) noexcept;

    
    /**
     * @brief 안전한 랜덤 키 생성 (AES-128용)
     * @return 16바이트 랜덤 키
     * @throws std::runtime_error 키 생성 실패 시
     */
    static std::vector<unsigned char> GenerateRandomKey();
    
    /**
     * @brief 안전한 랜덤 IV 생성
     * @return 16바이트 랜덤 IV
     * @throws std::runtime_error IV 생성 실패 시
     */
    static std::vector<unsigned char> GenerateRandomIV();

private:
    // High-Performance AES Context (내부 구현)
    class AESContext {
    public:
        AESContext();
        ~AESContext();
        
        // 포인터 기반 내부 구현
        bool EncryptAES128(const unsigned char* plaintext, size_t plaintext_size,
                          const unsigned char* key,
                          const unsigned char* iv,
                          unsigned char* ciphertext, size_t max_ciphertext_size) noexcept;
        
        bool DecryptAES128(const unsigned char* ciphertext, size_t ciphertext_size,
                          const unsigned char* key,
                          const unsigned char* iv,
                          unsigned char* plaintext, size_t max_plaintext_size,
                          size_t* actual_plaintext_size) noexcept;
                          
    private:
        EVP_CIPHER_CTX* ctx_;
    };
    
    // Thread-local Context 관리 (내부 구현)
    static AESContext& GetThreadLocalContext();

};