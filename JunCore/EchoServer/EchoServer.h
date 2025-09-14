#pragma once
#include "../JunCore/network/Server.h"
#include "../JunCore/protocol/UnifiedPacketHeader.h"
#include "echo_message.pb.h"

class EchoServer : public Server 
{
public:
	EchoServer(std::shared_ptr<IOCPManager> manager);
	~EchoServer();

protected:
	void OnClientJoin(Session* session) override;
	void OnClientLeave(Session* session) override;

	// Server 전용 가상함수 구현
	bool OnConnectionRequest(in_addr clientIP, WORD clientPort) override;
	void OnServerStart() override;
	void OnServerStop() override;
	
	// NetBase 패킷 핸들러 등록 (순수 가상함수 구현)
	void RegisterPacketHandlers() override;

private:
	Session* currentHandlingSession_ = nullptr;
	
	// 패킷별 핸들러 함수들 (PacketTest.cpp의 람다 대신 멤버 함수)
	void HandleEchoRequest(Session& _session, const echo::EchoRequest& request);
};