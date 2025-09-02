#pragma once
#include "../JunCore/network/NetBase.h"
#include "../JunCore/network/ServerSocketManager.h"

class EchoServer : public NetBase 
{
public:
	EchoServer();
	~EchoServer();

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
	// 기존 인터페이스 (호환성 유지용)
	bool OnConnectionRequest(in_addr IP, WORD Port);
	void OnServerStop();
	void OnError(int errorcode);
	

private:
	std::unique_ptr<ServerSocketManager> socketManager;  // 서버 소켓 관리
};