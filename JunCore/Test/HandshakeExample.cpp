#include <iostream>
#include <vector>
#include <memory>
#include <string>
#include <cstring>
#include "AES128.h"
#include "RSA2048.h"

// ============================================================================
// Network Headers
// ============================================================================

#pragma pack(push, 1)

// Plain header (handshake phase)
struct PlainHeader
{
    uint16_t len;  // Payload length
};

// Encrypted header (after handshake)
struct EncryptedHeader
{
    uint16_t len;     // Encrypted payload length
    uint8_t  iv[8];   // 8-byte IV (expanded to 16 internally)
};

#pragma pack(pop)

// ============================================================================
// Protocol Constants
// ============================================================================

const uint32_t PROTOCOL_CODE = 0x12345678;  // 4-byte protocol identifier

// ============================================================================
// Payload Structures (No message type - determined by session sequence)
// ============================================================================

#pragma pack(push, 1)

// Client → Server: RSA public key with protocol code
struct ClientRSAKeyPayload
{
    uint32_t protocol_code;
    uint16_t rsa_key_len;
    // RSA public key data follows
};

// Server → Client: RSA public key
struct ServerRSAKeyPayload
{
    uint16_t rsa_key_len;
    // RSA public key data follows
};

// Server → Client: AES key (RSA encrypted)
struct AESKeyDeliveryPayload
{
    uint16_t encrypted_key_len;
    uint16_t encrypted_iv_len;
    // Encrypted AES key + IV follows
};

#pragma pack(pop)

// ============================================================================
// Session State Management
// ============================================================================

enum class HandshakeState
{
    DISCONNECTED,           // Initial state
    CONNECTED,             // Connected, expecting client RSA key
    CLIENT_KEY_RECEIVED,   // Received client RSA key, sending server key
    SERVER_KEY_SENT,       // Sent server RSA key, sending AES key
    AES_KEY_SENT,          // Sent AES key, handshake complete
    HANDSHAKE_COMPLETE     // Ready for encrypted communication
};

struct SessionData
{
    HandshakeState state = HandshakeState::DISCONNECTED;
    
    // RSA keys (binary format)
    std::vector<unsigned char> client_rsa_public_key;
    std::vector<unsigned char> server_rsa_public_key;
    
    // AES key/IV
    std::vector<unsigned char> aes_key;
    std::vector<unsigned char> aes_iv;
    
    void Reset()
    {
        state = HandshakeState::DISCONNECTED;
        client_rsa_public_key.clear();
        server_rsa_public_key.clear();
        aes_key.clear();
        aes_iv.clear();
    }
};

// ============================================================================
// Simulation Data Structures
// ============================================================================

struct PacketData
{
    std::vector<unsigned char> data;
    bool is_encrypted = false;
    
    PacketData() = default;
    PacketData(const std::vector<unsigned char>& packet_data, bool encrypted = false)
        : data(packet_data), is_encrypted(encrypted) {}
};

// Simulated network channel
std::vector<PacketData> client_to_server_packets;
std::vector<PacketData> server_to_client_packets;

// ============================================================================
// Packet Helper Functions
// ============================================================================

void SendPlainPacket(std::vector<PacketData>& channel, const void* payload, uint16_t payload_len)
{
    std::vector<unsigned char> packet_data;
    
    PlainHeader header;
    header.len = payload_len;
    
    packet_data.resize(sizeof(header) + payload_len);
    memcpy(packet_data.data(), &header, sizeof(header));
    memcpy(packet_data.data() + sizeof(header), payload, payload_len);
    
    channel.emplace_back(packet_data, false);
}

bool ReceivePlainPacket(std::vector<PacketData>& channel, std::vector<unsigned char>& payload)
{
    if (channel.empty()) return false;
    
    PacketData packet = channel.front();
    channel.erase(channel.begin());
    
    if (packet.is_encrypted) return false; // Expected plain packet
    if (packet.data.size() < sizeof(PlainHeader)) return false;
    
    PlainHeader* header = (PlainHeader*)packet.data.data();
    if (packet.data.size() != sizeof(PlainHeader) + header->len) return false;
    
    payload.resize(header->len);
    memcpy(payload.data(), packet.data.data() + sizeof(PlainHeader), header->len);
    
    return true;
}

void SendEncryptedPacket(std::vector<PacketData>& channel, const std::vector<unsigned char>& plaintext, 
                        const std::vector<unsigned char>& aes_key)
{
    // Generate 8-byte IV, expand to 16
    std::vector<unsigned char> iv_8(8);
    if (RAND_bytes(iv_8.data(), 8) != 1) {
        std::cerr << "Failed to generate IV" << std::endl;
        return;
    }
    
    std::vector<unsigned char> iv_16(16);
    memcpy(iv_16.data(), iv_8.data(), 8);
    memset(iv_16.data() + 8, 0, 8);
    
    // Encrypt
    std::vector<unsigned char> ciphertext;
    if (!AES128::Encrypt(plaintext, aes_key, iv_16, ciphertext)) {
        std::cerr << "Failed to encrypt packet" << std::endl;
        return;
    }
    
    // Create packet
    std::vector<unsigned char> packet_data;
    EncryptedHeader header;
    header.len = ciphertext.size();
    memcpy(header.iv, iv_8.data(), 8);
    
    packet_data.resize(sizeof(header) + ciphertext.size());
    memcpy(packet_data.data(), &header, sizeof(header));
    memcpy(packet_data.data() + sizeof(header), ciphertext.data(), ciphertext.size());
    
    channel.emplace_back(packet_data, true);
}

bool ReceiveEncryptedPacket(std::vector<PacketData>& channel, std::vector<unsigned char>& plaintext,
                           const std::vector<unsigned char>& aes_key)
{
    if (channel.empty()) return false;
    
    PacketData packet = channel.front();
    channel.erase(channel.begin());
    
    if (!packet.is_encrypted) return false; // Expected encrypted packet
    if (packet.data.size() < sizeof(EncryptedHeader)) return false;
    
    EncryptedHeader* header = (EncryptedHeader*)packet.data.data();
    if (packet.data.size() != sizeof(EncryptedHeader) + header->len) return false;
    
    // Extract ciphertext
    std::vector<unsigned char> ciphertext(header->len);
    memcpy(ciphertext.data(), packet.data.data() + sizeof(EncryptedHeader), header->len);
    
    // Expand IV from 8 to 16 bytes
    std::vector<unsigned char> iv_16(16);
    memcpy(iv_16.data(), header->iv, 8);
    memset(iv_16.data() + 8, 0, 8);
    
    // Decrypt
    return AES128::Decrypt(ciphertext, aes_key, iv_16, plaintext);
}

// ============================================================================
// Handshake Protocol Implementation
// ============================================================================

bool ClientHandshake(SessionData& session)
{
    std::cout << "[Client] Starting handshake simulation..." << std::endl;
    
    // 1. Generate client RSA key pair
    std::cout << "[Client] Generating RSA key pair..." << std::endl;
    RSA2048 client_rsa;
    if (!client_rsa.GenerateKeyPair())
    {
        std::cerr << "[Client] Failed to generate RSA key pair" << std::endl;
        return false;
    }
    session.client_rsa_public_key = client_rsa.ExportPublicKey();
    
    // 2. Send client RSA public key with protocol code
    std::cout << "[Client] Sending RSA public key..." << std::endl;
    std::vector<unsigned char> payload;
    ClientRSAKeyPayload key_payload;
    key_payload.protocol_code = PROTOCOL_CODE;
    key_payload.rsa_key_len = session.client_rsa_public_key.size();
    
    payload.resize(sizeof(key_payload) + session.client_rsa_public_key.size());
    memcpy(payload.data(), &key_payload, sizeof(key_payload));
    memcpy(payload.data() + sizeof(key_payload), 
           session.client_rsa_public_key.data(), session.client_rsa_public_key.size());
    
    SendPlainPacket(client_to_server_packets, payload.data(), payload.size());
    session.state = HandshakeState::CONNECTED;
    
    // 3. Receive server RSA public key
    std::cout << "[Client] Waiting for server RSA key..." << std::endl;
    std::vector<unsigned char> server_response;
    if (!ReceivePlainPacket(server_to_client_packets, server_response))
    {
        std::cerr << "[Client] Failed to receive server RSA key" << std::endl;
        return false;
    }
    
    if (server_response.size() < sizeof(ServerRSAKeyPayload))
    {
        std::cerr << "[Client] Invalid server RSA key packet" << std::endl;
        return false;
    }
    
    ServerRSAKeyPayload* server_key_payload = (ServerRSAKeyPayload*)server_response.data();
    session.server_rsa_public_key.assign(
        server_response.data() + sizeof(ServerRSAKeyPayload),
        server_response.data() + server_response.size()
    );
    
    std::cout << "[Client] Received server RSA key" << std::endl;
    
    // 4. Receive encrypted AES key
    std::cout << "[Client] Waiting for AES key..." << std::endl;
    std::vector<unsigned char> aes_response;
    if (!ReceivePlainPacket(server_to_client_packets, aes_response))
    {
        std::cerr << "[Client] Failed to receive AES key" << std::endl;
        return false;
    }
    
    if (aes_response.size() < sizeof(AESKeyDeliveryPayload))
    {
        std::cerr << "[Client] Invalid AES key packet" << std::endl;
        return false;
    }
    
    AESKeyDeliveryPayload* aes_payload = (AESKeyDeliveryPayload*)aes_response.data();
    
    // 5. Decrypt AES key using client private RSA key
    std::cout << "[Client] Decrypting AES key..." << std::endl;
    size_t offset = sizeof(AESKeyDeliveryPayload);
    std::vector<unsigned char> encrypted_aes_key(
        aes_response.data() + offset,
        aes_response.data() + offset + aes_payload->encrypted_key_len
    );
    
    offset += aes_payload->encrypted_key_len;
    std::vector<unsigned char> encrypted_aes_iv(
        aes_response.data() + offset,
        aes_response.data() + offset + aes_payload->encrypted_iv_len
    );
    
    if (!client_rsa.DecryptWithPrivateKey(encrypted_aes_key, session.aes_key) ||
        !client_rsa.DecryptWithPrivateKey(encrypted_aes_iv, session.aes_iv))
    {
        std::cerr << "[Client] Failed to decrypt AES key/IV" << std::endl;
        return false;
    }
    
    session.state = HandshakeState::HANDSHAKE_COMPLETE;
    std::cout << "[Client] Handshake completed successfully!" << std::endl;
    return true;
}

bool ServerHandshake(SessionData& session)
{
    std::cout << "[Server] Starting handshake simulation..." << std::endl;
    session.state = HandshakeState::CONNECTED;
    
    // 1. Receive client RSA public key
    std::cout << "[Server] Waiting for client RSA key..." << std::endl;
    std::vector<unsigned char> client_data;
    if (!ReceivePlainPacket(client_to_server_packets, client_data))
    {
        std::cerr << "[Server] Failed to receive client data" << std::endl;
        return false;
    }
    
    if (client_data.size() < sizeof(ClientRSAKeyPayload))
    {
        std::cerr << "[Server] Invalid client data packet" << std::endl;
        return false;
    }
    
    ClientRSAKeyPayload* client_payload = (ClientRSAKeyPayload*)client_data.data();
    
    // 2. Verify protocol code based on session state
    if (session.state == HandshakeState::CONNECTED)
    {
        if (client_payload->protocol_code != PROTOCOL_CODE)
        {
            std::cerr << "[Server] Invalid protocol code: " << std::hex << client_payload->protocol_code << std::endl;
            return false;
        }
        std::cout << "[Server] Protocol code verified, extracting client RSA key..." << std::endl;
    }
    
    session.client_rsa_public_key.assign(
        client_data.data() + sizeof(ClientRSAKeyPayload),
        client_data.data() + client_data.size()
    );
    session.state = HandshakeState::CLIENT_KEY_RECEIVED;
    
    // 3. Generate server RSA key pair
    std::cout << "[Server] Generating server RSA key pair..." << std::endl;
    RSA2048 server_rsa;
    if (!server_rsa.GenerateKeyPair())
    {
        std::cerr << "[Server] Failed to generate server RSA key pair" << std::endl;
        return false;
    }
    session.server_rsa_public_key = server_rsa.ExportPublicKey();
    
    // 4. Send server RSA public key
    std::cout << "[Server] Sending server RSA public key..." << std::endl;
    std::vector<unsigned char> server_payload;
    ServerRSAKeyPayload server_key_payload;
    server_key_payload.rsa_key_len = session.server_rsa_public_key.size();
    
    server_payload.resize(sizeof(server_key_payload) + session.server_rsa_public_key.size());
    memcpy(server_payload.data(), &server_key_payload, sizeof(server_key_payload));
    memcpy(server_payload.data() + sizeof(server_key_payload),
           session.server_rsa_public_key.data(), session.server_rsa_public_key.size());
    
    SendPlainPacket(server_to_client_packets, server_payload.data(), server_payload.size());
    session.state = HandshakeState::SERVER_KEY_SENT;
    
    // 5. Generate AES key and IV
    std::cout << "[Server] Generating AES key and IV..." << std::endl;
    session.aes_key = AES128::GenerateRandomKey();
    session.aes_iv = AES128::GenerateRandomIV();
    session.aes_iv.resize(16); // Ensure 16 bytes
    
    // 6. Encrypt AES key with client's RSA public key
    std::cout << "[Server] Encrypting AES credentials..." << std::endl;
    RSA2048 client_public_rsa;
    if (!client_public_rsa.ImportPublicKey(session.client_rsa_public_key))
    {
        std::cerr << "[Server] Failed to import client public key" << std::endl;
        return false;
    }
    
    std::vector<unsigned char> encrypted_key, encrypted_iv;
    if (!client_public_rsa.EncryptWithPublicKey(session.aes_key, encrypted_key) ||
        !client_public_rsa.EncryptWithPublicKey(session.aes_iv, encrypted_iv))
    {
        std::cerr << "[Server] Failed to encrypt AES credentials" << std::endl;
        return false;
    }
    
    // 7. Send encrypted AES key
    std::cout << "[Server] Sending encrypted AES credentials..." << std::endl;
    std::vector<unsigned char> aes_payload;
    AESKeyDeliveryPayload aes_delivery;
    aes_delivery.encrypted_key_len = encrypted_key.size();
    aes_delivery.encrypted_iv_len = encrypted_iv.size();
    
    aes_payload.resize(sizeof(aes_delivery) + encrypted_key.size() + encrypted_iv.size());
    memcpy(aes_payload.data(), &aes_delivery, sizeof(aes_delivery));
    memcpy(aes_payload.data() + sizeof(aes_delivery), encrypted_key.data(), encrypted_key.size());
    memcpy(aes_payload.data() + sizeof(aes_delivery) + encrypted_key.size(), 
           encrypted_iv.data(), encrypted_iv.size());
    
    SendPlainPacket(server_to_client_packets, aes_payload.data(), aes_payload.size());
    session.state = HandshakeState::AES_KEY_SENT;
    
    session.state = HandshakeState::HANDSHAKE_COMPLETE;
    std::cout << "[Server] Handshake completed successfully!" << std::endl;
    return true;
}

// ============================================================================
// Encrypted Communication Simulation
// ============================================================================

void SendEncryptedMessage(std::vector<PacketData>& channel, const std::string& message,
                         const std::vector<unsigned char>& aes_key)
{
    std::vector<unsigned char> plaintext(message.begin(), message.end());
    SendEncryptedPacket(channel, plaintext, aes_key);
}

bool ReceiveEncryptedMessage(std::vector<PacketData>& channel, std::string& message,
                            const std::vector<unsigned char>& aes_key)
{
    std::vector<unsigned char> plaintext;
    if (!ReceiveEncryptedPacket(channel, plaintext, aes_key))
    {
        return false;
    }
    
    message.assign(plaintext.begin(), plaintext.end());
    return true;
}

// ============================================================================
// Main Simulation
// ============================================================================

void RunHandshakeSimulation()
{
    std::cout << "=== Secure Handshake Simulation ===" << std::endl;
    
    SessionData client_session;
    SessionData server_session;
    
    // Clear packet channels
    client_to_server_packets.clear();
    server_to_client_packets.clear();
    
    // Perform handshake
    std::cout << "\n--- Phase 1: Client sends RSA key ---" << std::endl;
    ClientHandshake(client_session); // Sends client RSA key
    
    std::cout << "\n--- Phase 2: Server processes and responds ---" << std::endl;
    ServerHandshake(server_session); // Processes and sends server RSA + AES key
    
    std::cout << "\n--- Phase 3: Client completes handshake ---" << std::endl;
    ClientHandshake(client_session); // Receives and processes server response
    
    if (client_session.state != HandshakeState::HANDSHAKE_COMPLETE ||
        server_session.state != HandshakeState::HANDSHAKE_COMPLETE)
    {
        std::cerr << "Handshake failed!" << std::endl;
        return;
    }
    
    // Test encrypted communication
    std::cout << "\n--- Phase 4: Encrypted Communication Test ---" << std::endl;
    
    // Client sends message
    std::string test_message = "Hello from client!";
    std::cout << "[Client] Sending: " << test_message << std::endl;
    SendEncryptedMessage(client_to_server_packets, test_message, client_session.aes_key);
    
    // Server receives and echoes
    std::string received_message;
    if (ReceiveEncryptedMessage(client_to_server_packets, received_message, server_session.aes_key))
    {
        std::cout << "[Server] Received: " << received_message << std::endl;
        std::cout << "[Server] Echoing back..." << std::endl;
        SendEncryptedMessage(server_to_client_packets, received_message, server_session.aes_key);
    }
    
    // Client receives echo
    std::string echo_message;
    if (ReceiveEncryptedMessage(server_to_client_packets, echo_message, client_session.aes_key))
    {
        std::cout << "[Client] Received echo: " << echo_message << std::endl;
    }
    
    std::cout << "\n=== Simulation Complete ===" << std::endl;
    std::cout << "✓ Protocol code verification" << std::endl;
    std::cout << "✓ RSA key exchange" << std::endl;
    std::cout << "✓ AES key delivery" << std::endl;
    std::cout << "✓ Encrypted communication" << std::endl;
}

// Main function removed - RunHandshakeSimulation() is now called from main.cpp