#pragma once
#include "../JunCore/network/NetworkTypes.h"

class EchoClient : public NetClient 
{
public:
	EchoClient(const char* systemFile, const char* client);
	~EchoClient();

public:
	// NetworkEngine 기본 순수 가상 함수들 (클라이언트에서는 사용하지 않지만 구현 필요)
	bool OnConnectionRequest(in_addr ip, WORD port) override { return false; }
	void OnClientJoin(SessionId sessionId) override {}
	void OnRecv(SessionId sessionId, PacketBuffer* contentsPacket) override {}
	void OnClientLeave(SessionId sessionId) override {}
	void OnServerStop() override {}

	// 클라이언트 전용 함수들 - virtual 함수 override
	void OnConnect() override;
	void OnRecv(PacketBuffer* csContentsPacket) override;
	void OnDisconnect() override;
	void OnClientStop() override;
};