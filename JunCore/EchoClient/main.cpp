#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include "network/EchoClient.h"
#include "packet/packet.pb.h"
using namespace std;

int main()
{
	// 서버 연결 정보 설정
	std::string _ip = "127.0.0.1";
	uint16 _port = 8085;

	// 워커 스레드 개수 설정
	int32 networkThreads = 1;

	// 클라이언트 시작
	if (!sEchoClient.StartClient(networkThreads))
	{
		std::cerr << "Failed to start EchoClient" << std::endl;
		return -1;
	}

	// 서버 연결
	if (!sEchoClient.Connect(_ip, _port))
	{
		std::cerr << "Failed to connect to server" << std::endl;
		return -1;
	}

	// 연결 완료될 때까지 대기
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));

	std::cout << "EchoClient started. Type messages to send to server (type 'quit' to exit):" << std::endl;

	// 콘솔 입력 처리
	std::string input;
	while (std::getline(std::cin, input))
	{
		if (input == "quit")
		{
			break;
		}

		// 세션이 연결되어 있는지 확인
		if (sEchoClient.session_ptr_ == nullptr)
		{
			std::cout << "Not connected to server. Type 'quit' to exit." << std::endl;
			continue;
		}

		// 입력된 메시지를 서버로 전송
		PacketLib::UG_ECHO_REQ _echo_req;
		_echo_req.set_echo(input);
		sEchoClient.session_ptr_->SendPacket(static_cast<int32>(PacketLib::PACKET_ID::PACKET_ID_UG_ECHO_REQ), _echo_req);

		std::cout << "Message sent: " << input << std::endl;
	}

	// 클라이언트 정리
	sEchoClient.Disconnect();
	sEchoClient.StopClient();

	return 0;
}