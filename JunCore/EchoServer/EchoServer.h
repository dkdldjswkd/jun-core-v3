#pragma once
#include "../JunCore/network/Server.h"

class EchoServer : public Server 
{
public:
	EchoServer();
	~EchoServer();

public:

protected:
	// NetBase 순수 가상 함수 구현 (핵심 비즈니스 로직)
	void OnRecv(Session* session, PacketBuffer* packet) override;
	void OnClientJoin(Session* session) override;
	void OnClientLeave(Session* session) override;

	// Server 전용 가상함수 구현
	bool OnConnectionRequest(in_addr clientIP, WORD clientPort) override;
	void OnServerStart() override;
	void OnServerStop() override;

public:
	// 편의 함수 (기존 인터페이스와의 호환성)
	void OnError(int errorcode);
};