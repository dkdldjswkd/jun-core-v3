#pragma once
#include "../core/WindowsIncludes.h"
#include "../core/base.h"
#include "Session.h"
#include "IOCPManager.h"
#include "WSAInitializer.h"
#include <memory>
#include <iostream>
#include <string>
#include <vector>
#include "../protocol/UnifiedPacketHeader.h"
#include <functional>
#include <unordered_map>

class NetBase
{
private:
    friend class Session;
	friend class IOCPManager;

protected:
    using PacketHandler = std::function<void(Session&, const std::vector<char>&)>;

public:
    NetBase(std::shared_ptr<IOCPManager> manager);
    virtual ~NetBase();

public:
	// 엔진 초기화
	void Initialize();
	bool IsInitialized() const noexcept;

protected:
    // 패킷 핸들러 등록
    template<typename T>
    void RegisterPacketHandler(std::function<void(Session&, const T&)> handler);
    virtual void RegisterPacketHandlers() = 0;

	// 세션 연결 종료
    virtual void OnDisconnect(Session* session) = 0;

private:
    // 세션 유효성 검사
    bool IsSessionValid(Session* session) const;
	void OnPacketReceived(Session* session, uint32_t packet_id, const std::vector<char>& payload);

protected:
	std::shared_ptr<IOCPManager> iocpManager;
	std::unordered_map<uint32_t, PacketHandler> packet_handlers_;

	bool initialized_ = false;
};

inline NetBase::NetBase(std::shared_ptr<IOCPManager> manager) : iocpManager(manager)
{
    if (!manager) 
    {
        throw std::invalid_argument("IOCPManager cannot be null");
    }
    
    if (!WSAInitializer::Initialize()) 
    {
        throw std::runtime_error("WSAStartup_ERROR");
    }
}

inline NetBase::~NetBase()
{
    iocpManager.reset();
    WSAInitializer::Cleanup();
}

inline void NetBase::OnPacketReceived(Session* session, uint32_t packet_id, const std::vector<char>& payload)
{
    auto it = packet_handlers_.find(packet_id);
    if (it != packet_handlers_.end()) 
    {
        it->second(*session, payload);  // 등록된 핸들러 실행
    }
    else 
    {
        LOG_WARN("No handler registered for packet ID: %d", packet_id);
    }
}

template<typename T>
void NetBase::RegisterPacketHandler(std::function<void(Session&, const T&)> handler)
{
    const std::string type_name = T::descriptor()->full_name();
    const uint32_t packet_id = fnv1a(type_name.c_str());
    
    LOG_DEBUG("Registering packet handler for %s (ID: %d)", type_name.c_str(), packet_id);
    
    packet_handlers_[packet_id] = [handler](Session& session, const std::vector<char>& payload)
    {
        T message;
        if (message.ParseFromArray(payload.data(), payload.size()))
        {
            handler(session, message);
        }
        else
        {
            LOG_ERROR("Failed to parse packet for type: %s", T::descriptor()->full_name().c_str());
        }
    };
}

inline void NetBase::Initialize()
{
    if (!initialized_) 
    {
        RegisterPacketHandlers();
        initialized_ = true;
    }
}

inline bool NetBase::IsInitialized() const noexcept
{
    return initialized_;
}

inline bool NetBase::IsSessionValid(Session* session) const
{
	return session != nullptr && session->sock_ != INVALID_SOCKET;
}