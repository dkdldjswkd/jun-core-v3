#pragma once
#include "../JunCore/network/Client.h"
#include "../JunCore/protocol/DirectProtobuf.h"
#include "echo_message.pb.h"

class EchoClient : public Client 
{
public:
	EchoClient();
	~EchoClient();

public:
	// Protobuf 메시지 송신 편의 함수
	bool SendEchoRequest(const std::string& message);

protected:
	void OnRecv(Session* session, PacketBuffer* packet) override;
	void OnClientJoin(Session* session) override;
	void OnDisconnect() override;
};