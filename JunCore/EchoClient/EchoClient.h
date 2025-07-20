#pragma once
#include "../JunCore/NetClient.h"

class EchoClient : public NetClient 
{
public:
	EchoClient(const char* systemFile, const char* client);
	~EchoClient();

public:
	void OnConnect();
	void OnRecv(PacketBuffer* csContentsPacket);
	void OnDisconnect();
	void OnClientStop();
};