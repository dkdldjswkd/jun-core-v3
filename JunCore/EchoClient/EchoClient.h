#pragma once
#include "../JunCore/network/NetBase.h"

class EchoClient : public NetBase 
{
public:
	EchoClient();
	~EchoClient();

public:
	// NetBase 순수 가상 함수 구현
	void Start() override;
	void Stop() override;

protected:
	// NetBase 순수 가상 함수 구현
	void OnRecv(Session* session, PacketBuffer* packet) override;
	void OnClientJoin(Session* session) override;
	void OnClientLeave(Session* session) override;

public:
	// 클라이언트 전용 함수들
	bool Connect(const char* serverIP, int port);
	bool SendMessage(const char* message);

private:
	Session* clientSession = nullptr;
	bool connected = false;
};