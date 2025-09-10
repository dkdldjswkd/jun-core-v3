#pragma once
#include "../core/WindowsIncludes.h"
#include "../core/base.h"
#include "Session.h"
#include "IOCPManager.h"
#include <memory>
#include <iostream>
#include <string>
#include <vector>
#include "../protocol/UnifiedPacketHeader.h"

class NetBase
{
public:
    NetBase();
    virtual ~NetBase();

protected:
    std::shared_ptr<IOCPManager> iocpManager;

public:
    // IOCP 매니저 연결 인터페이스
    void AttachIOCPManager(std::shared_ptr<IOCPManager> manager);
    void DetachIOCPManager();
    bool IsIOCPManagerAttached() const noexcept;


protected:
	template<typename T>
    bool SendPacket(Session& session, const T& packet);
    void DisconnectSession(Session* session);

private:
    // 세션 유효성 검사
    bool IsSessionValid(Session* session) const 
    {
        return session != nullptr && session->sock_ != INVALID_SOCKET;
    }
};

// 인라인 구현
inline NetBase::NetBase()
{
    WSADATA wsaData;
    int wsaResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (0 != wsaResult) 
    {
        printf("WSAStartup failed with error: %d\n", wsaResult);
        throw std::runtime_error("WSAStartup_ERROR");
    }
}

inline NetBase::~NetBase()
{
    DetachIOCPManager();
    WSACleanup();
}

inline void NetBase::AttachIOCPManager(std::shared_ptr<IOCPManager> manager)
{
    iocpManager = manager;
}

inline void NetBase::DetachIOCPManager()
{
    if (iocpManager) 
    {
        iocpManager.reset();
    }
}

inline bool NetBase::IsIOCPManagerAttached() const noexcept
{
    return iocpManager != nullptr && iocpManager->IsValid();
}

template<typename T>
bool NetBase::SendPacket(Session& session, const T& packet)
{
    // 세션 유효성 검사
    if (!IsSessionValid(&session)) 
    {
        return false;
    }
    
    // 1. 프로토버프 메시지 직렬화 크기 계산
    size_t payload_size = packet.ByteSizeLong();
    size_t total_size   = UNIFIED_HEADER_SIZE + payload_size;
    
    // 2. 패킷 ID 생성 (프로토버프 타입명 해시)
    const std::string& type_name = packet.GetDescriptor()->full_name();
    uint32_t packet_id = fnv1a(type_name.c_str());
    
    // 3. 패킷 버퍼 생성
    std::vector<char>* packet_data = new std::vector<char>(total_size);
    
    // 4. 헤더 설정
    UnifiedPacketHeader* header = reinterpret_cast<UnifiedPacketHeader*>(packet_data->data());
    header->length = static_cast<uint32_t>(total_size);
    header->packet_id = packet_id;
    
    // 5. 페이로드 직렬화
    if (!packet.SerializeToArray(packet_data->data() + sizeof(UnifiedPacketHeader), payload_size))
    {
        std::cout << "[SEND_PACKET] Failed to serialize packet" << std::endl;
        return false;
    }
    
    std::cout << "[SEND_PACKET] Sending packet ID: " << packet_id << " Size: " << total_size << " bytes, session : " << (session.session_id_.SESSION_UNIQUE & 0xFFFF) << std::endl;
    
	// 6. 송신 큐에 추가
	session.send_q_.Enqueue(packet_data);

	// 7. Send flag에 따른 패킷 송신
	if (InterlockedExchange8((char*)&session.send_flag_, true) == false) 
    {
		if (iocpManager)
		{
			iocpManager->PostAsyncSend(&session);
		}
	}

	return true;
}

inline void NetBase::DisconnectSession(Session* session)
{
    if (IsSessionValid(session)) 
    {
        session->DisconnectSession();
    }
}