#pragma once
#include "../JunCore/network/Client.h"
#include "../JunCore/protocol/UnifiedPacketHeader.h"
#include "echo_message.pb.h"

class EchoClient : public Client 
{
public:
	EchoClient(std::shared_ptr<IOCPManager> manager);
	~EchoClient();

protected:
	void OnUserDisconnect(User* user) override;
	
	// NetBase 패킷 핸들러 등록 (순수 가상함수 구현)
	void RegisterPacketHandlers() override;
};