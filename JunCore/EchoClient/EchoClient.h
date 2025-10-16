#pragma once
#include "../JunCore/network/Client.h"
#include "../JunCore/protocol/UnifiedPacketHeader.h"
#include "echo_message.pb.h"

class EchoClient : public Client
{
public:
	EchoClient(std::shared_ptr<IOCPManager> manager, const char* serverIP, WORD port, int targetConnectionCount = 1);
	~EchoClient();

	User* GetUser() const { return connectedUser; }

protected:
	void OnConnectComplete(User* user, bool success) override;

	// NetBase 패킷 핸들러 등록 (순수 가상함수 구현)
	void RegisterPacketHandlers() override;

private:
	User* connectedUser = nullptr;
};