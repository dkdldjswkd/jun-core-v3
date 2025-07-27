#include "CryptoExample.h"
#include <iostream>
#include <string>
#include <iomanip>
#include <sstream>

// 바이트 배열을 16진수 문자열로 변환
std::string BytesToHex(const std::vector<unsigned char>& bytes)
{
    std::stringstream ss;
    for (size_t i = 0; i < bytes.size(); ++i) {
        if (i > 0) ss << " ";
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(bytes[i]);
    }
    return ss.str();
}

void TestAES()
{
    std::cout << "\n======= AES-128 Interactive Test =======" << std::endl;
    std::cout << "Enter a string: ";
    
    std::string input;
    std::getline(std::cin, input);
    
    if (input.empty()) {
        std::cout << "Input is empty." << std::endl;
        return;
    }
    
    // Convert string to byte array
    std::vector<unsigned char> plaintext(input.begin(), input.end());
    
    std::cout << "\n=== Original Data ===" << std::endl;
    std::cout << "String: \"" << input << "\"" << std::endl;
    std::cout << "Size: " << plaintext.size() << " bytes" << std::endl;
    std::cout << "Hex: " << BytesToHex(plaintext) << std::endl;
    
    try {
        // Generate key and IV
        auto key = AES128::GenerateRandomKey();
        auto iv = AES128::GenerateRandomIV();
        
        std::cout << "\n=== Encryption Key/IV ===" << std::endl;
        std::cout << "Key (16bytes): " << BytesToHex(key) << std::endl;
        std::cout << "IV (16bytes): " << BytesToHex(iv) << std::endl;
        
        // Encryption
        std::vector<unsigned char> ciphertext;
        if (AES128::Encrypt(plaintext, key, iv, ciphertext)) {
            std::cout << "\n=== Encryption Result ===" << std::endl;
            std::cout << "Status: Success" << std::endl;
            std::cout << "Size: " << ciphertext.size() << " bytes" << std::endl;
            std::cout << "Hex: " << BytesToHex(ciphertext) << std::endl;
            
            // Decryption
            std::vector<unsigned char> decrypted;
            if (AES128::Decrypt(ciphertext, key, iv, decrypted)) {
                std::string decrypted_string(decrypted.begin(), decrypted.end());
                
                std::cout << "\n=== Decryption Result ===" << std::endl;
                std::cout << "Status: Success" << std::endl;
                std::cout << "Size: " << decrypted.size() << " bytes" << std::endl;
                std::cout << "Hex: " << BytesToHex(decrypted) << std::endl;
                std::cout << "String: \"" << decrypted_string << "\"" << std::endl;
                
                // Integrity check
                std::cout << "\n=== Integrity Check ===" << std::endl;
                if (input == decrypted_string) {
                    std::cout << "Result: OK - Success (matches original)" << std::endl;
                } else {
                    std::cout << "Result: ERROR - Failed (data corruption)" << std::endl;
                }
            } else {
                std::cout << "\n[ERROR] Decryption failed!" << std::endl;
            }
        } else {
            std::cout << "\n[ERROR] Encryption failed!" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cout << "\n[ERROR] " << e.what() << std::endl;
    }
    
    std::cout << "\n=== AES128 Class Features ===" << std::endl;
    std::cout << "- High-Performance AES-128-CBC Encryption" << std::endl;
    std::cout << "- Thread-local Context Reuse" << std::endl;
    std::cout << "- Minimal Memory Allocation Overhead" << std::endl;
    std::cout << "- General Purpose Usage" << std::endl;
}