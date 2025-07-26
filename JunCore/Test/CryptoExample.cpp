#include "CryptoExample.h"
#include<openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/aes.h>
#include <iostream>

bool CryptoExample::EncryptAES256(const std::vector<unsigned char>& plaintext,
	const std::vector<unsigned char>& key,
	std::vector<unsigned char>& ciphertext,
	std::vector<unsigned char>& iv)
{
	EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
	if (!ctx) return false;

	iv = GenerateRandomIV();

	if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key.data(), iv.data()) != 1) {
		EVP_CIPHER_CTX_free(ctx);
		return false;
	}

	int len;
	int ciphertext_len;
	ciphertext.resize(plaintext.size() + AES_BLOCK_SIZE);

	if (EVP_EncryptUpdate(ctx, ciphertext.data(), &len, plaintext.data(), static_cast<int>(plaintext.size())) != 1) {
		EVP_CIPHER_CTX_free(ctx);
		return false;
	}
	ciphertext_len = len;

	if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len) != 1) {
		EVP_CIPHER_CTX_free(ctx);
		return false;
	}
	ciphertext_len += len;

	ciphertext.resize(ciphertext_len);
	EVP_CIPHER_CTX_free(ctx);
	return true;
}

bool CryptoExample::DecryptAES256(const std::vector<unsigned char>& ciphertext,
	const std::vector<unsigned char>& key,
	const std::vector<unsigned char>& iv,
	std::vector<unsigned char>& plaintext)
{
	EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
	if (!ctx) return false;

	if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key.data(), iv.data()) != 1) {
		EVP_CIPHER_CTX_free(ctx);
		return false;
	}

	int len;
	int plaintext_len;
	plaintext.resize(ciphertext.size());

	if (EVP_DecryptUpdate(ctx, plaintext.data(), &len, ciphertext.data(), static_cast<int>(ciphertext.size())) != 1) {
		EVP_CIPHER_CTX_free(ctx);
		return false;
	}
	plaintext_len = len;

	if (EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len) != 1) {
		EVP_CIPHER_CTX_free(ctx);
		return false;
	}
	plaintext_len += len;

	plaintext.resize(plaintext_len);
	EVP_CIPHER_CTX_free(ctx);
	return true;
}

std::vector<unsigned char> CryptoExample::GenerateRandomKey()
{
	std::vector<unsigned char> key(32); // AES-256 key size
	if (RAND_bytes(key.data(), 32) != 1) {
		std::cerr << "Failed to generate random key" << std::endl;
	}
	return key;
}

std::vector<unsigned char> CryptoExample::GenerateRandomIV()
{
	std::vector<unsigned char> iv(AES_BLOCK_SIZE); // 16 bytes
	if (RAND_bytes(iv.data(), AES_BLOCK_SIZE) != 1) {
		std::cerr << "Failed to generate random IV" << std::endl;
	}
	return iv;
}

void CryptoExample::RunCryptoExample()
{
	std::cout << "\n=== AES-256 Encryption/Decryption Example ===" << std::endl;
	
	// Generate key and prepare data
	auto key = CryptoExample::GenerateRandomKey();
	std::string message = "Hello, this is a secret message!";
	std::vector<unsigned char> plaintext(message.begin(), message.end());
	
	std::cout << "Original message: " << message << std::endl;
	
	// Encrypt
	std::vector<unsigned char> ciphertext;
	std::vector<unsigned char> iv;
	
	if (CryptoExample::EncryptAES256(plaintext, key, ciphertext, iv)) {
		std::cout << "Encryption successful! Ciphertext size: " << ciphertext.size() << " bytes" << std::endl;
		
		// Decrypt
		std::vector<unsigned char> decrypted;
		if (CryptoExample::DecryptAES256(ciphertext, key, iv, decrypted)) {
			std::string decrypted_message(decrypted.begin(), decrypted.end());
			std::cout << "Decrypted message: " << decrypted_message << std::endl;
			
			if (message == decrypted_message) {
				std::cout << "SUCCESS: Original and decrypted messages match!" << std::endl;
			} else {
				std::cout << "ERROR: Messages do not match!" << std::endl;
			}
		} else {
			std::cout << "Decryption failed!" << std::endl;
		}
	} else {
		std::cout << "Encryption failed!" << std::endl;
	}
}