#pragma once
#include "../JunCore/network/NetBase_New.h"
#include "../JunCore/network/ServerSocketManager.h"

class EchoServer : public NetBase 
{
public:
	EchoServer(const char* systemFile, const char* server);
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
	
	// 모니터링 인터페이스 (기존 코드 호환성)
	DWORD GetSessionCount() const { 
		return socketManager ? socketManager->GetCurrentSessionCount() : NetBase::GetSessionCount(); 
	}
	DWORD GetAcceptTotal() const { return acceptTotal; }
	DWORD GetSendTPS() const { return NetBase::GetPacketTPS(); }
	DWORD GetRecvTPS() const { return NetBase::GetPacketTPS(); }

private:
	DWORD acceptTotal = 0;  // 총 접속 수 카운터
	std::unique_ptr<ServerSocketManager> socketManager;  // 서버 소켓 관리
};