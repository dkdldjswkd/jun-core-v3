#include "EchoServer.h"

// ���� 5:29 2023-02-21

EchoServer::EchoServer(const char* systemFile, const char* server) : NetServer(systemFile, server)
{
}

EchoServer::~EchoServer() {
}

void EchoServer::OnRecv(SessionId session_id, PacketBuffer* cs_contentsPacket) {
	//------------------------------
	// var set
	//------------------------------
	PacketBuffer* sc_contentsPacket = PacketBuffer::Alloc();

	//------------------------------
	// SC Contents Packet ����
	//------------------------------
	auto cs_contentsPacket_len = cs_contentsPacket->GetPayloadSize();
	char* payloadData = new char[cs_contentsPacket_len];
	cs_contentsPacket->GetData(payloadData, cs_contentsPacket_len);
	sc_contentsPacket->PutData(payloadData, cs_contentsPacket_len);
	delete[] payloadData;
	cs_contentsPacket->MoveRp(cs_contentsPacket_len);

	//------------------------------
	// Send Packet
	//------------------------------
	SendPacket(session_id, sc_contentsPacket);

	//------------------------------
	// Release Func
	//------------------------------
	PacketBuffer::Free(sc_contentsPacket);
	return;
}

bool EchoServer::OnConnectionRequest(in_addr ip, WORD port){
	//printf("[Accept] IP(%s), PORT(%u) \n", inet_ntoa(ip), port);
	return true;
}

void EchoServer::OnClientJoin(SessionId session_id){
	//------------------------------
	// var set
	//------------------------------
	PacketBuffer* sc_packet = PacketBuffer::Alloc();

	//------------------------------
	// sc packet ����
	//------------------------------
	*sc_packet << 0x7fffffffffffffff;

	//------------------------------
	// Send Packet
	//------------------------------
	SendPacket(session_id, sc_packet);

	//------------------------------
	// Release Func
	//------------------------------
	PacketBuffer::Free(sc_packet);
	return;
}

void EchoServer::OnClientLeave(SessionId session_id){
}

void EchoServer::OnServerStop(){
}

void EchoServer::OnError(int errorcode){
}