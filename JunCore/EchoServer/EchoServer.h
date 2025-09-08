#pragma once
#include "../JunCore/network/Server.h"
#include "../JunCore/protocol/UnifiedPacketHeader.h"
#include "../JunCore/protocol/DirectProtobuf.h"  // g_direct_packet_handler용
#include "echo_message.pb.h"

class EchoServer : public Server 
{
public:
	EchoServer();
	~EchoServer();

protected:
	void OnClientJoin(Session* session) override;
	void OnClientLeave(Session* session) override;

	// Server 전용 가상함수 구현
	bool OnConnectionRequest(in_addr clientIP, WORD clientPort) override;
	void OnServerStart() override;
	void OnServerStop() override;

private:
	// PacketTest.cpp 방식: 현재 처리 중인 세션 저장
	Session* currentHandlingSession_ = nullptr;
	
	// 패킷별 핸들러 함수들 (PacketTest.cpp의 람다 대신 멤버 함수)
	void HandleEchoRequest(const echo::EchoRequest& request);

public:
	// 편의 함수 (기존 인터페이스와의 호환성)
	void OnError(int errorcode);
};