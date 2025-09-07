#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "EchoServer.h"

EchoServer::EchoServer() : Server()
{
	// PacketTest.cpp 방식: 패킷별 핸들러 등록
	RegisterDirectPacketHandler<echo::EchoRequest>(
		[this](const echo::EchoRequest& request) {
			this->HandleEchoRequest(request);
		}
	);
}

EchoServer::~EchoServer() 
{
}

// PacketTest.cpp의 세션 저장 방식 - 현재 처리 중인 세션을 멤버로 관리
void EchoServer::HandleEchoRequest(const echo::EchoRequest& request)
{
	std::cout << "[SERVER] HandleEchoRequest: " << request.message() << std::endl;
	
	// EchoResponse 생성
	echo::EchoResponse response;
	response.set_message(request.message());
	response.set_timestamp(GetTickCount64());
	
	// 응답 패킷 직렬화
	std::vector<char> response_data = SerializeDirectPacket(response);
	
	// 현재 처리 중인 세션에 직접 raw 데이터 응답 전송 (PacketBuffer 없이)
	if (currentHandlingSession_) {
		bool success = SendRawData(currentHandlingSession_, response_data);
		std::cout << "[SERVER] Sent EchoResponse via raw send: " << response.message() 
				  << " (success=" << success << ")" << std::endl;
	}
	else {
		std::cout << "[SERVER] No current session to send response!" << std::endl;
	}
}

void EchoServer::OnRecv(Session* session, PacketBuffer* packet)
{
	try {
		// 현재 처리 중인 세션 저장 (PacketTest.cpp의 전역 변수 대신 멤버 변수 사용)
		currentHandlingSession_ = session;
		
		// 패킷 길이 검사
		auto packet_len = packet->GetPayloadSize();
		if (packet_len <= 0 || packet_len > 8000) {
			LOG_ERROR("Invalid packet size: %d", packet_len);
			PacketBuffer::Free(packet);
			currentHandlingSession_ = nullptr;
			return;
		}

		std::cout << "[SERVER] OnRecv: session=" << (session->session_id_.SESSION_UNIQUE & 0xFFFF) 
				  << ", packet_len=" << packet_len << std::endl;

		// raw 데이터 추출 (이미 LAN 헤더가 제거된 DirectPacket 데이터)
		std::vector<char> packet_data(packet_len);
		packet->GetData(packet_data.data(), packet_len);
		packet->MoveRp(packet_len);
		PacketBuffer::Free(packet);

		// PacketTest.cpp 방식으로 패킷 처리
		bool processed = ProcessDirectPacket(packet_data.data(), packet_len);
		if (!processed) {
			std::cout << "[SERVER] Failed to process direct packet" << std::endl;
		}
		
		// 세션 정보 초기화
		currentHandlingSession_ = nullptr;
	}
	catch (const std::exception& e) {
		LOG_ERROR("Exception in OnRecv: %s", e.what());
		PacketBuffer::Free(packet);
		currentHandlingSession_ = nullptr;
	}
	catch (...) {
		LOG_ERROR("Unknown exception in OnRecv");
		PacketBuffer::Free(packet);
		currentHandlingSession_ = nullptr;
	}
}

bool EchoServer::OnConnectionRequest(in_addr clientIP, WORD clientPort) {
	// 모든 클라이언트 허용
	return true;
}

void EchoServer::OnClientJoin(Session* session) 
{
	LOG_INFO("[%04X][JOIN] Client connected", (session->session_id_.SESSION_UNIQUE & 0xFFFF));
}

void EchoServer::OnClientLeave(Session* session) 
{
	printf("[%04X][LEAVE] Client disconnected\n", (session->session_id_.SESSION_UNIQUE & 0xFFFF));
}

void EchoServer::OnServerStart()
{
	LOG_INFO("EchoServer started successfully!");
}

void EchoServer::OnServerStop() 
{
	LOG_INFO("EchoServer stopped successfully!");
}

void EchoServer::OnError(int errorcode) 
{
}