#pragma once
#include <vector>
#include <string>
#include <memory>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/err.h>

/**
 * @brief 고성능 RSA-2048 암호화 클래스
 * 
 * - 키 교환 전용으로 설계 (1회성 사용)
 * - AES 키 암호화/복호화에 최적화
 * - OpenSSL 기반 구현
 */
class RSA2048
{
public:
    RSA2048();
    ~RSA2048();

    // 복사/이동 금지 (RSA 키는 고유해야 함)
    RSA2048(const RSA2048&) = delete;
    RSA2048& operator=(const RSA2048&) = delete;
    RSA2048(RSA2048&&) = delete;
    RSA2048& operator=(RSA2048&&) = delete;

public:
    /**
     * @brief RSA 키 쌍 생성 (2048bit)
     * @return 성공 시 true, 실패 시 false
     * @note 생성에 시간이 걸릴 수 있음 (보통 100ms~1초)
     */
    bool GenerateKeyPair();

    /**
     * @brief 공개키를 바이너리 형태로 내보내기
     * @return 공개키 바이너리 데이터 (DER 형식)
     * @throws std::runtime_error 키가 없거나 내보내기 실패 시
     */
    std::vector<unsigned char> ExportPublicKey() const;

    /**
     * @brief 바이너리 공개키 가져오기
     * @param publicKeyData 공개키 바이너리 데이터 (DER 형식)
     * @return 성공 시 true, 실패 시 false
     */
    bool ImportPublicKey(const std::vector<unsigned char>& publicKeyData);

    /**
     * @brief 공개키로 데이터 암호화 (상대방 공개키 사용)
     * @param plaintext 암호화할 데이터 (최대 245바이트)
     * @param ciphertext 암호화된 데이터 (출력, 256바이트)
     * @return 성공 시 true, 실패 시 false
     * @note AES 키(32바이트) 암호화 용도로 설계됨
     */
    bool EncryptWithPublicKey(const std::vector<unsigned char>& plaintext,
                             std::vector<unsigned char>& ciphertext) const;

    /**
     * @brief 개인키로 데이터 복호화 (자신의 개인키 사용)
     * @param ciphertext 암호화된 데이터 (256바이트)
     * @param plaintext 복호화된 데이터 (출력)
     * @return 성공 시 true, 실패 시 false
     */
    bool DecryptWithPrivateKey(const std::vector<unsigned char>& ciphertext,
                              std::vector<unsigned char>& plaintext) const;

    /**
     * @brief 키 쌍이 유효한지 확인
     * @return 개인키와 공개키 모두 있으면 true
     */
    bool HasKeyPair() const;

    /**
     * @brief 공개키만 있는지 확인
     * @return 공개키만 있으면 true (타인의 공개키)
     */
    bool HasPublicKeyOnly() const;

    /**
     * @brief OpenSSL 에러 메시지 가져오기
     * @return 최근 OpenSSL 에러 문자열
     */
    static std::string GetLastError();

private:
    RSA* rsa_keypair_;      // 자신의 키 쌍 (개인키 + 공개키)
    RSA* rsa_public_only_;  // 상대방의 공개키만

    // OpenSSL 초기화 (정적)
    static void InitializeOpenSSL();
    static bool openssl_initialized_;
};

// RSA 암호화 상수
namespace RSAConstants {
    constexpr int KEY_SIZE_BITS = 2048;                    // RSA 키 크기
    constexpr int KEY_SIZE_BYTES = KEY_SIZE_BITS / 8;      // 256바이트
    constexpr int MAX_ENCRYPT_SIZE = KEY_SIZE_BYTES - 11;  // PKCS#1 패딩으로 인한 제한 (245바이트)
    constexpr int PUBLIC_EXPONENT = 65537;                // 표준 공개 지수 (0x10001)
}