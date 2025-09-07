#pragma once
#include "../JunCore/network/Client.h"

class EchoClient : public Client 
{
public:
	EchoClient();
	~EchoClient();

protected:
	void OnRecv(Session* session, PacketBuffer* packet) override;
	void OnClientJoin(Session* session) override;
	void OnDisconnect() override;
};