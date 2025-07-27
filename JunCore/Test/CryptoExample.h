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
     * @brief 데이터 암호화
     * @param plaintext 원본 데이터
     * @param key AES-128 키 (16바이트)
     * @param iv 초기화 벡터 (16바이트)
     * @param ciphertext 암호화된 데이터 (출력)
     * @return 성공 시 true, 실패 시 false
     * 
     * @note IV는 호출자가 관리 (세션별 카운터 IV 또는 랜덤 IV)
     * @note 암호화된 데이터는 최대 원본크기 + 16바이트
     */
    static bool Encrypt(const std::vector<unsigned char>& plaintext,
                       const std::vector<unsigned char>& key,
                       const std::vector<unsigned char>& iv,
                       std::vector<unsigned char>& ciphertext) noexcept;
    
    /**
     * @brief 데이터 복호화
     * @param ciphertext 암호화된 데이터
     * @param key AES-128 키 (16바이트)
     * @param iv 초기화 벡터 (16바이트)
     * @param plaintext 복호화된 데이터 (출력)
     * @return 성공 시 true, 실패 시 false (데이터 변조 시 실패)
     * 
     * @note 데이터 무결성 검증 포함
     * @note 패딩은 자동으로 제거됨
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
        
        bool EncryptAES128(const std::vector<unsigned char>& plaintext,
                          const std::vector<unsigned char>& key,
                          const std::vector<unsigned char>& iv,
                          std::vector<unsigned char>& ciphertext) noexcept;
        
        bool DecryptAES128(const std::vector<unsigned char>& ciphertext,
                          const std::vector<unsigned char>& key,
                          const std::vector<unsigned char>& iv,
                          std::vector<unsigned char>& plaintext) noexcept;
                          
    private:
        EVP_CIPHER_CTX* ctx_;
    };
    
    // Thread-local Context 관리 (내부 구현)
    static AESContext& GetThreadLocalContext();

};