#pragma once
#include "../JunCore/network/Server.h"
#include "../JunCore/protocol/UnifiedPacketHeader.h"
#include "echo_message.pb.h"
#include <atomic>

class EchoServer : public Server 
{
public:
	EchoServer(std::shared_ptr<IOCPManager> manager);
	~EchoServer();

public:
	// 세션 모니터링 함수
	uint32_t GetCurrentSessions()	const { return currentSessions_.load();   };
	uint32_t GetTotalConnected()	const { return totalConnected_.load();	  };
	uint32_t GetTotalDisconnected() const { return totalDisconnected_.load(); };
	
private:
	// 패킷 핸들
	void HandleEchoRequest(User& user, const echo::EchoRequest& request);

	// 사용자 재정의 함수
	void OnSessionConnect(User* user) override;
	void OnSessionDisconnect(User* user) override;
	void OnServerStart() override;
	void OnServerStop() override;
	void RegisterPacketHandlers() override;

private:
	// 세션 모니터링 변수
	std::atomic<uint32_t> currentSessions_{0};      // 현재 접속중인 세션 수
	std::atomic<uint32_t> totalConnected_{0};       // 누적 연결된 세션 수  
	std::atomic<uint32_t> totalDisconnected_{0};    // 누적 끊어진 세션 수
};

//uint32_t EchoServer::GetCurrentSessions()	const { return currentSessions_.load();   }
//uint32_t EchoServer::GetTotalConnected()	const { return totalConnected_.load();	  }
//uint32_t EchoServer::GetTotalDisconnected() const { return totalDisconnected_.load(); }