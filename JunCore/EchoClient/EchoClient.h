#pragma once
#include "../JunCore/network/Client.h"
#include "../JunCore/protocol/UnifiedPacketHeader.h"
#include "../JunCore/protocol/DirectProtobuf.h"  // g_direct_packet_handler용
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
	void OnClientJoin(Session* session) override;
	void OnDisconnect() override;
};