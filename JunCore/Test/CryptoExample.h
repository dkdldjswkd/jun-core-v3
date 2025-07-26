#pragma once
#include <vector>
#include <string>

class CryptoExample
{
public:
    static bool EncryptAES256(const std::vector<unsigned char>& plaintext,
                              const std::vector<unsigned char>& key,
                              std::vector<unsigned char>& ciphertext,
                              std::vector<unsigned char>& iv);

    static bool DecryptAES256(const std::vector<unsigned char>& ciphertext,
                              const std::vector<unsigned char>& key,
                              const std::vector<unsigned char>& iv,
                              std::vector<unsigned char>& plaintext);

    static std::vector<unsigned char> GenerateRandomKey();
    static std::vector<unsigned char> GenerateRandomIV();
    
    static void RunCryptoExample();
};