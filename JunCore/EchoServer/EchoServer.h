﻿#pragma once
#include "../JunCore/NetServer.h"

class EchoServer : public NetServer 
{
public:
	EchoServer(const char* systemFile, const char* server);
	~EchoServer();

public:
	bool OnConnectionRequest(in_addr IP, WORD Port);
	void OnClientJoin(SessionId session_id);
	void OnRecv(SessionId session_id, PacketBuffer* contents_packet);
	void OnClientLeave(SessionId session_id);
	void OnServerStop();
	void OnError(int errorcode);
};