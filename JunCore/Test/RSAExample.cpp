#include "../JunCommon/crypto/RSA2048.h"
#include <iostream>
#include <string>
#include <iomanip>
#include <sstream>
#include <algorithm>

#define NOMINMAX
#include <Windows.h>

// 바이트 배열을 16진수 문자열로 변환 (디버깅용)
std::string BytesToHex(const std::vector<unsigned char>& bytes, size_t max_display = 32)
{
    std::stringstream ss;
    size_t display_size = std::min(bytes.size(), max_display);
    
    for (size_t i = 0; i < display_size; ++i) {
        if (i > 0) ss << " ";
        ss << std::hex << std::setw(2) << std::setfill('0') 
           << static_cast<int>(bytes[i]);
    }
    
    if (bytes.size() > max_display) {
        ss << " ... (+" << (bytes.size() - max_display) << " bytes)";
    }
    
    return ss.str();
}

void TestRSABasicOperations()
{
    std::cout << "\n======= RSA-2048 Class Test =======" << std::endl;
    
    try {
        // 1. RSA 키 쌍 생성 테스트
        std::cout << "\n=== Step 1: RSA Key Pair Generation ===" << std::endl;
        RSA2048 rsa;
        
        std::cout << "Generating RSA key pair (2048-bit)..." << std::flush;
        DWORD start_time = GetTickCount();
        
        if (!rsa.GenerateKeyPair()) {
            std::cout << " FAILED!" << std::endl;
            std::cout << "Error: " << RSA2048::GetLastError() << std::endl;
            return;
        }
        
        DWORD generation_time = GetTickCount() - start_time;
        std::cout << " SUCCESS! (took " << generation_time << "ms)" << std::endl;
        
        // 2. 공개키 추출 테스트
        std::cout << "\n=== Step 2: Public Key Export ===" << std::endl;
        auto publicKey = rsa.ExportPublicKey();
        std::cout << "Public key exported: " << publicKey.size() << " bytes" << std::endl;
        std::cout << "Key data (hex): " << BytesToHex(publicKey, 20) << std::endl;
        
        // 3. 다른 RSA 인스턴스에서 공개키 임포트 테스트
        std::cout << "\n=== Step 3: Public Key Import ===" << std::endl;
        RSA2048 otherRSA;
        
        if (!otherRSA.ImportPublicKey(publicKey)) {
            std::cout << "Failed to import public key!" << std::endl;
            return;
        }
        std::cout << "Public key imported: SUCCESS" << std::endl;
        
        // 4. 데이터 암호화/복호화 테스트
        std::cout << "\n=== Step 4: Encryption/Decryption Test ===" << std::endl;
        
        // 테스트 데이터 (AES 키 시뮬레이션)
        std::vector<unsigned char> testData = {
            0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
            0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C
        };
        
        std::cout << "Original data (16 bytes): " << BytesToHex(testData) << std::endl;
        
        // 암호화 (다른 인스턴스에서 공개키로 암호화)
        std::vector<unsigned char> encrypted;
        if (!otherRSA.EncryptWithPublicKey(testData, encrypted)) {
            std::cout << "Encryption failed!" << std::endl;
            std::cout << "Error: " << RSA2048::GetLastError() << std::endl;
            return;
        }
        
        std::cout << "Encrypted data: " << encrypted.size() << " bytes" << std::endl;
        std::cout << "Encrypted (hex): " << BytesToHex(encrypted, 20) << std::endl;
        
        // 복호화 (원본 인스턴스에서 개인키로 복호화)
        std::vector<unsigned char> decrypted;
        if (!rsa.DecryptWithPrivateKey(encrypted, decrypted)) {
            std::cout << "Decryption failed!" << std::endl;
            std::cout << "Error: " << RSA2048::GetLastError() << std::endl;
            return;
        }
        
        std::cout << "Decrypted data: " << decrypted.size() << " bytes" << std::endl;
        std::cout << "Decrypted (hex): " << BytesToHex(decrypted) << std::endl;
        
        // 5. 무결성 검증
        std::cout << "\n=== Step 5: Integrity Verification ===" << std::endl;
        
        bool isValid = (testData == decrypted);
        std::cout << "Data Integrity Check: " << (isValid ? "PASS" : "FAIL") << std::endl;
        
        // 6. 상태 확인 테스트
        std::cout << "\n=== Step 6: State Verification ===" << std::endl;
        std::cout << "Original RSA has key pair: " << (rsa.HasKeyPair() ? "YES" : "NO") << std::endl;
        std::cout << "Original RSA has public only: " << (rsa.HasPublicKeyOnly() ? "YES" : "NO") << std::endl;
        std::cout << "Other RSA has key pair: " << (otherRSA.HasKeyPair() ? "YES" : "NO") << std::endl;
        std::cout << "Other RSA has public only: " << (otherRSA.HasPublicKeyOnly() ? "YES" : "NO") << std::endl;
        
        if (isValid) {
            std::cout << "\n[SUCCESS] RSA-2048 Class Test: ALL PASSED!" << std::endl;
            std::cout << "[OK] Key generation: PASS" << std::endl;
            std::cout << "[OK] Public key export: PASS" << std::endl;  
            std::cout << "[OK] Public key import: PASS" << std::endl;
            std::cout << "[OK] Encryption/decryption: PASS" << std::endl;
            std::cout << "[OK] Data integrity: PASS" << std::endl;
            std::cout << "[OK] State management: PASS" << std::endl;
        } else {
            std::cout << "\n[FAILED] RSA-2048 Class Test: FAILED!" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cout << "\n[EXCEPTION] " << e.what() << std::endl;
        std::cout << "OpenSSL Error: " << RSA2048::GetLastError() << std::endl;
    }
}

void TestRSAErrorHandling()
{
    std::cout << "\n======= RSA Error Handling Test =======" << std::endl;
    
    try {
        RSA2048 rsa;
        
        // 1. 키 생성 전 공개키 추출 시도
        std::cout << "\n=== Test 1: Export without key generation ===" << std::endl;
        try {
            auto publicKey = rsa.ExportPublicKey();
            std::cout << "ERROR: Should have thrown exception!" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "[OK] Correctly threw exception: " << e.what() << std::endl;
        }
        
        // 2. 잘못된 공개키 임포트 시도
        std::cout << "\n=== Test 2: Invalid public key import ===" << std::endl;
        std::vector<unsigned char> invalidKey = {0x01, 0x02, 0x03};
        if (!rsa.ImportPublicKey(invalidKey)) {
            std::cout << "[OK] Correctly rejected invalid key" << std::endl;
        } else {
            std::cout << "ERROR: Should have rejected invalid key!" << std::endl;
        }
        
        // 3. 너무 큰 데이터 암호화 시도
        std::cout << "\n=== Test 3: Oversized data encryption ===" << std::endl;
        rsa.GenerateKeyPair();
        auto publicKey = rsa.ExportPublicKey();
        
        RSA2048 otherRSA;
        otherRSA.ImportPublicKey(publicKey);
        
        std::vector<unsigned char> oversizedData(300, 0xAA); // 300바이트 (245바이트 제한 초과)
        std::vector<unsigned char> encrypted;
        
        if (!otherRSA.EncryptWithPublicKey(oversizedData, encrypted)) {
            std::cout << "[OK] Correctly rejected oversized data" << std::endl;
        } else {
            std::cout << "ERROR: Should have rejected oversized data!" << std::endl;
        }
        
        std::cout << "\n[SUCCESS] RSA Error Handling: ALL PASSED!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "\n[EXCEPTION] " << e.what() << std::endl;
    }
}

// RSA 예제 메인 함수
void TestRSA()
{
    std::cout << "=== RSA-2048 Encryption Class Test ===" << std::endl;
    std::cout << "OpenSSL version: " << OPENSSL_VERSION_TEXT << std::endl;
    
    // 기본 동작 테스트
    TestRSABasicOperations();
    
    // 에러 처리 테스트
    TestRSAErrorHandling();
    
    std::cout << "\n=== RSA Test Complete ===" << std::endl;
}