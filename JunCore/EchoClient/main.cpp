#include <iostream>
#include <string>
#include "EchoClient.h"
#include "../JunCommon/system/CrashDump.h"
using namespace std;

int main() 
{
	static CrashDump dump;

	EchoClient client;
	client.Start();
	printf("EchoClient started\n");

	// 인터랙티브 채팅 모드
	Sleep(1000); // 서버 연결 완료 대기
	
	cout << "=== 채팅을 시작합니다. 'exit' 입력시 종료 ===" << endl;
	
	while (true) 
	{
		string input;
		cout << ">> ";
		
		if (!getline(cin, input)) {
			cout << "\n입력 오류 또는 EOF. 종료합니다..." << endl;
			break;
		}
		
		if (input.empty()) {
			continue; // 빈 입력은 무시
		}

		if(input == "exit") {
			cout << "채팅을 종료합니다." << endl;
			break; 
		}

		// 서버에 메시지 전송
		client.SendMessage(input.c_str());
	}

	cout << "클라이언트를 종료합니다..." << endl;
	client.Stop();
	cout << "클라이언트가 종료되었습니다." << endl;
}