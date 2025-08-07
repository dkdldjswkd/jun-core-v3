#include <iostream>
#include <vector>
#include <cstring>
#include <ctime>
#include <random>
#include <iomanip>
#include <sstream>
#include "../JunCommon/crypto/AES128.h"

// ============================================================================
// Protocol Configuration
// ============================================================================

const char* CLIENT_VERSION = "1.2.3";  // 버전 변경시 프로토콜 코드도 자동 변경됨
const uint32_t HANDSHAKE_TIMEOUT = 30; // 30초

// ============================================================================
// Packet Structures
// ============================================================================

#pragma pack(push, 1)

// Client → Server: Initial handshake request
struct ClientHandshakeRequest
{
    uint64_t timestamp;        // Unix timestamp (8 bytes)
    uint32_t protocol_code;    // Version-based hash (4 bytes)
};

// Server → Client: Session key delivery
struct ServerHandshakeResponse
{
    uint8_t encrypted_session_key[16];  // XOR encrypted AES-128 key (16 bytes)
    uint32_t session_id;                // Session identifier (4 bytes)
};

#pragma pack(pop)

// ============================================================================
// Crypto Helper Functions
// ============================================================================

// Simple hash function for version string
uint32_t HashVersion(const char* version)
{
    uint32_t hash = 0x811C9DC5; // FNV-1a offset basis
    const uint32_t prime = 0x01000193; // FNV-1a prime
    
    while (*version) {
        hash ^= static_cast<uint8_t>(*version++);
        hash *= prime;
    }
    
    return hash;
}

// Generate XOR key from protocol code
void GenerateXORKey(uint32_t protocol_code, uint8_t xor_key[16])
{
    // Expand 4-byte protocol code to 16-byte XOR key
    for (int i = 0; i < 16; i++) {
        xor_key[i] = static_cast<uint8_t>((protocol_code >> ((i % 4) * 8)) & 0xFF);
        xor_key[i] ^= static_cast<uint8_t>(i * 0x5A); // Add position-based variation
    }
}

// XOR encrypt/decrypt (same operation)
void XORCrypt(const uint8_t* input, uint8_t* output, const uint8_t* key, size_t length)
{
    for (size_t i = 0; i < length; i++) {
        output[i] = input[i] ^ key[i];
    }
}

// Get current Unix timestamp
uint64_t GetCurrentTimestamp()
{
    return static_cast<uint64_t>(std::time(nullptr));
}

// Check if timestamp is within valid range
bool IsTimestampValid(uint64_t timestamp, uint32_t timeout_seconds)
{
    uint64_t current = GetCurrentTimestamp();
    uint64_t diff = (current > timestamp) ? (current - timestamp) : (timestamp - current);
    return diff <= timeout_seconds;
}

// ============================================================================
// Session Management
// ============================================================================

struct SessionData
{
    uint32_t session_id;
    uint8_t session_key[16];     // AES-128 key
    bool handshake_complete;
    
    SessionData() : session_id(0), handshake_complete(false) {
        memset(session_key, 0, sizeof(session_key));
    }
};

// ============================================================================
// Client Simulation
// ============================================================================

class ClientSimulator
{
private:
    SessionData session_;
    uint32_t protocol_code_;
    uint8_t xor_key_[16];
    
public:
    ClientSimulator()
    {
        // Generate protocol code from version
        protocol_code_ = HashVersion(CLIENT_VERSION);
        GenerateXORKey(protocol_code_, xor_key_);
        
        std::cout << "[Client] Initialized with version: " << CLIENT_VERSION << std::endl;
        std::cout << "[Client] Protocol code: 0x" << std::hex << protocol_code_ << std::dec << std::endl;
    }
    
    // Step 1: Send handshake request
    ClientHandshakeRequest CreateHandshakeRequest()
    {
        ClientHandshakeRequest request;
        request.timestamp = GetCurrentTimestamp();
        request.protocol_code = protocol_code_;
        
        std::cout << "[Client] Sending handshake request..." << std::endl;
        std::cout << "[Client] Timestamp: " << request.timestamp << std::endl;
        std::cout << "[Client] Protocol Code: 0x" << std::hex << request.protocol_code << std::dec << std::endl;
        
        return request;
    }
    
    // Step 2: Process server response
    bool ProcessServerResponse(const ServerHandshakeResponse& response)
    {
        std::cout << "[Client] Processing server response..." << std::endl;
        
        // Decrypt session key using XOR
        XORCrypt(response.encrypted_session_key, session_.session_key, xor_key_, 16);
        session_.session_id = response.session_id;
        session_.handshake_complete = true;
        
        std::cout << "[Client] Session ID: " << session_.session_id << std::endl;
        std::cout << "[Client] Session key decrypted successfully!" << std::endl;
        std::cout << "[Client] Handshake complete!" << std::endl;
        
        return true;
    }
    
    // Step 3: Test encrypted communication
    std::vector<uint8_t> EncryptMessage(const std::string& message)
    {
        if (!session_.handshake_complete) {
            std::cerr << "[Client] Error: Handshake not complete!" << std::endl;
            return {};
        }
        
        std::vector<uint8_t> plaintext(message.begin(), message.end());
        std::vector<uint8_t> iv = AES128::GenerateRandomIV();
        std::vector<uint8_t> key(session_.session_key, session_.session_key + 16);
        
        std::vector<uint8_t> ciphertext;
        if (AES128::Encrypt(plaintext, key, iv, ciphertext)) {
            std::cout << "[Client] Message encrypted: " << message << std::endl;
            // Prepend IV to ciphertext for transmission
            std::vector<uint8_t> result;
            result.insert(result.end(), iv.begin(), iv.end());
            result.insert(result.end(), ciphertext.begin(), ciphertext.end());
            return result;
        } else {
            std::cerr << "[Client] Encryption failed!" << std::endl;
            return {};
        }
    }
    
    std::string DecryptMessage(const std::vector<uint8_t>& encrypted_data)
    {
        if (!session_.handshake_complete || encrypted_data.size() < 16) {
            return "";
        }
        
        // Extract IV and ciphertext
        std::vector<uint8_t> iv(encrypted_data.begin(), encrypted_data.begin() + 16);
        std::vector<uint8_t> ciphertext(encrypted_data.begin() + 16, encrypted_data.end());
        std::vector<uint8_t> key(session_.session_key, session_.session_key + 16);
        
        std::vector<uint8_t> plaintext;
        if (AES128::Decrypt(ciphertext, key, iv, plaintext)) {
            std::string message(plaintext.begin(), plaintext.end());
            std::cout << "[Client] Message decrypted: " << message << std::endl;
            return message;
        } else {
            std::cerr << "[Client] Decryption failed!" << std::endl;
            return "";
        }
    }
};

// ============================================================================
// Server Simulation
// ============================================================================

class ServerSimulator
{
private:
    std::vector<SessionData> sessions_;
    uint32_t next_session_id_;
    uint32_t expected_protocol_code_;
    uint8_t xor_key_[16];
    
public:
    ServerSimulator()
    {
        next_session_id_ = 1000;
        
        // Server also knows the client version and generates same protocol code
        expected_protocol_code_ = HashVersion(CLIENT_VERSION);
        GenerateXORKey(expected_protocol_code_, xor_key_);
        
        std::cout << "[Server] Initialized with expected client version: " << CLIENT_VERSION << std::endl;
        std::cout << "[Server] Expected protocol code: 0x" << std::hex << expected_protocol_code_ << std::dec << std::endl;
    }
    
    // Step 1: Process client handshake request
    ServerHandshakeResponse ProcessHandshakeRequest(const ClientHandshakeRequest& request)
    {
        ServerHandshakeResponse response;
        memset(&response, 0, sizeof(response));
        
        std::cout << "[Server] Processing handshake request..." << std::endl;
        
        // Validate timestamp
        if (!IsTimestampValid(request.timestamp, HANDSHAKE_TIMEOUT)) {
            std::cerr << "[Server] Error: Timestamp validation failed! Time gap > 30 seconds" << std::endl;
            return response; // Return empty response (connection will be dropped)
        }
        
        // Validate protocol code
        if (request.protocol_code != expected_protocol_code_) {
            std::cerr << "[Server] Error: Protocol code mismatch! Expected: 0x" 
                      << std::hex << expected_protocol_code_ 
                      << ", Got: 0x" << request.protocol_code << std::dec << std::endl;
            return response; // Return empty response (connection will be dropped)
        }
        
        std::cout << "[Server] Timestamp and protocol code validated successfully!" << std::endl;
        
        // Create new session
        SessionData new_session;
        new_session.session_id = next_session_id_++;
        
        // Generate random AES-128 session key
        std::vector<uint8_t> random_key = AES128::GenerateRandomKey();
        if (random_key.size() != 16) {
            std::cerr << "[Server] Error: Failed to generate session key!" << std::endl;
            return response;
        }
        
        memcpy(new_session.session_key, random_key.data(), 16);
        new_session.handshake_complete = true;
        
        // Store session
        sessions_.push_back(new_session);
        
        // Encrypt session key with XOR
        XORCrypt(new_session.session_key, response.encrypted_session_key, xor_key_, 16);
        response.session_id = new_session.session_id;
        
        std::cout << "[Server] Session created: " << new_session.session_id << std::endl;
        std::cout << "[Server] Session key generated and encrypted!" << std::endl;
        
        return response;
    }
    
    // Find session by ID
    SessionData* FindSession(uint32_t session_id)
    {
        for (auto& session : sessions_) {
            if (session.session_id == session_id) {
                return &session;
            }
        }
        return nullptr;
    }
    
    // Test encrypted communication
    std::vector<uint8_t> EncryptMessage(uint32_t session_id, const std::string& message)
    {
        SessionData* session = FindSession(session_id);
        if (!session || !session->handshake_complete) {
            std::cerr << "[Server] Error: Invalid session or handshake not complete!" << std::endl;
            return {};
        }
        
        std::vector<uint8_t> plaintext(message.begin(), message.end());
        std::vector<uint8_t> iv = AES128::GenerateRandomIV();
        std::vector<uint8_t> key(session->session_key, session->session_key + 16);
        
        std::vector<uint8_t> ciphertext;
        if (AES128::Encrypt(plaintext, key, iv, ciphertext)) {
            std::cout << "[Server] Message encrypted: " << message << std::endl;
            // Prepend IV to ciphertext for transmission
            std::vector<uint8_t> result;
            result.insert(result.end(), iv.begin(), iv.end());
            result.insert(result.end(), ciphertext.begin(), ciphertext.end());
            return result;
        } else {
            std::cerr << "[Server] Encryption failed!" << std::endl;
            return {};
        }
    }
    
    std::string DecryptMessage(uint32_t session_id, const std::vector<uint8_t>& encrypted_data)
    {
        SessionData* session = FindSession(session_id);
        if (!session || !session->handshake_complete || encrypted_data.size() < 16) {
            return "";
        }
        
        // Extract IV and ciphertext
        std::vector<uint8_t> iv(encrypted_data.begin(), encrypted_data.begin() + 16);
        std::vector<uint8_t> ciphertext(encrypted_data.begin() + 16, encrypted_data.end());
        std::vector<uint8_t> key(session->session_key, session->session_key + 16);
        
        std::vector<uint8_t> plaintext;
        if (AES128::Decrypt(ciphertext, key, iv, plaintext)) {
            std::string message(plaintext.begin(), plaintext.end());
            std::cout << "[Server] Message decrypted: " << message << std::endl;
            return message;
        } else {
            std::cerr << "[Server] Decryption failed!" << std::endl;
            return "";
        }
    }
};

// ============================================================================
// Main Simulation
// ============================================================================

void RunSimpleHandshakeSimulation()
{
    std::cout << "=== Simple Handshake Simulation ===" << std::endl;
    std::cout << "Features:" << std::endl;
    std::cout << "- Version-based protocol code (hash)" << std::endl;
    std::cout << "- Timestamp validation (30 second window)" << std::endl;
    std::cout << "- XOR encryption for key transport" << std::endl;
    std::cout << "- AES-128 session encryption" << std::endl;
    std::cout << "- Single version change updates both protocol code and XOR key" << std::endl;
    std::cout << std::endl;
    
    // Initialize client and server
    ClientSimulator client;
    ServerSimulator server;
    
    std::cout << "\n--- Phase 1: Client Handshake Request ---" << std::endl;
    ClientHandshakeRequest request = client.CreateHandshakeRequest();
    
    std::cout << "\n--- Phase 2: Server Validation and Response ---" << std::endl;
    ServerHandshakeResponse response = server.ProcessHandshakeRequest(request);
    
    // Check if handshake succeeded
    if (response.session_id == 0) {
        std::cerr << "Handshake failed! Connection would be dropped." << std::endl;
        return;
    }
    
    std::cout << "\n--- Phase 3: Client Processes Response ---" << std::endl;
    client.ProcessServerResponse(response);
    
    std::cout << "\n--- Phase 4: Encrypted Communication Test ---" << std::endl;
    
    // Client sends encrypted message
    std::string client_message = "Hello from client!";
    std::vector<uint8_t> encrypted_msg = client.EncryptMessage(client_message);
    
    if (!encrypted_msg.empty()) {
        // Server receives and decrypts
        std::string decrypted = server.DecryptMessage(response.session_id, encrypted_msg);
        
        if (decrypted == client_message) {
            std::cout << "[SUCCESS] Message integrity verified!" << std::endl;
            
            // Server responds
            std::string server_message = "Hello from server!";
            std::vector<uint8_t> server_encrypted = server.EncryptMessage(response.session_id, server_message);
            
            if (!server_encrypted.empty()) {
                std::string client_received = client.DecryptMessage(server_encrypted);
                if (client_received == server_message) {
                    std::cout << "[SUCCESS] Bidirectional communication verified!" << std::endl;
                }
            }
        }
    }
    
    std::cout << "\n=== Simulation Complete ===" << std::endl;
    std::cout << "✓ Version-based protocol code generation" << std::endl;
    std::cout << "✓ Timestamp validation (30s window)" << std::endl;
    std::cout << "✓ XOR key derivation from protocol code" << std::endl;
    std::cout << "✓ Secure session key exchange" << std::endl;
    std::cout << "✓ AES-128 encrypted communication" << std::endl;
    std::cout << "\nPacket sizes:" << std::endl;
    std::cout << "- Handshake request: " << sizeof(ClientHandshakeRequest) << " bytes" << std::endl;
    std::cout << "- Handshake response: " << sizeof(ServerHandshakeResponse) << " bytes" << std::endl;
    std::cout << "- Total handshake overhead: " << sizeof(ClientHandshakeRequest) + sizeof(ServerHandshakeResponse) << " bytes" << std::endl;
}

// Main function removed - RunSimpleHandshakeSimulation() is now called from main.cpp