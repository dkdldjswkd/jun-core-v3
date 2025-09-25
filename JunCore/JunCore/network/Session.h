#pragma once
#include "../core/WindowsIncludes.h"
#include "../../JunCommon/container/LFQueue.h"
#include "../../JunCommon/container/LFStack.h"
#include "../../JunCommon/container/RingBuffer.h"
#include "../core/base.h"
#include "../protocol/UnifiedPacketHeader.h"
#include <vector>
#include <string>
#include <atomic>
#include <memory>
#include "IOCPManager.h"

constexpr int MAX_SEND_MSG = 100;

class Session;

enum class IOOperation
{
	IO_RECV,
	IO_SEND,
	IO_ACCEPT,
	IO_DISCONNECT
};

struct OverlappedEx
{
	OverlappedEx(std::shared_ptr<Session> session, IOOperation operation_)
		: operation_(operation_), session_(session)
	{}
	~OverlappedEx(){}

	OVERLAPPED overlapped_ = { 0, };
	std::shared_ptr<Session> session_;
	IOOperation operation_;
};

//------------------------------
// Session
//------------------------------
class IOCPManager; // Forward declaration
class NetBase;     // Forward declaration

class Session : public std::enable_shared_from_this<Session>
{
	friend class IOCPManager;

public:
	Session();
	~Session();
	
	// atomic 멤버 때문에 복사/이동 명시적 처리
	Session(const Session&) = delete;
	Session& operator=(const Session&) = delete;
	Session(Session&&) = delete;
	Session& operator=(Session&&) = delete;

public:
	// 소켓 정보
	SOCKET sock_ = INVALID_SOCKET;
	in_addr ip_;
	WORD port_;

	// flag
	bool send_flag_ = false;
	std::atomic<bool> pending_disconnect_ = false;

	// Send
	LFQueue<std::vector<char>*> send_q_;				// 송신 대기 큐 (raw 데이터)
	std::vector<char>* send_packet_arr_[MAX_SEND_MSG];	// 현재 전송중인 패킷 벡터들
	LONG send_packet_count_ = 0;						// 현재 전송중인 패킷 개수

	// Recv
	RingBuffer recv_buf_;

	// TimeOut
	DWORD last_recv_time_;

	// 암호화
	std::vector<char> aes_key_;

private:
	HANDLE h_iocp_ = INVALID_HANDLE_VALUE;  // IOCP 핸들 저장
	class NetBase* engine_ = nullptr;       // 이 세션을 소유한 엔진
	
	// Session Pool 직접 관리 (포인터 기반)
	std::vector<std::unique_ptr<Session>>* session_pool_ = nullptr;  // 자신이 속한 세션 풀
	LFStack<DWORD>* session_index_stack_ = nullptr; // 세션 인덱스 스택

public:
	void Set(SOCKET sock, in_addr ip, WORD port, NetBase* eng);
	void Release();  // 세션 정리 + Pool 반환 통합
	
	// Session Pool 직접 설정 (간단하고 명확함)
	void SetSessionPool(std::vector<std::unique_ptr<Session>>* pool, LFStack<DWORD>* indexStack);
	
	inline void SetIOCP(HANDLE iocp_handle) { h_iocp_ = iocp_handle; }
	inline HANDLE GetIOCP() { return h_iocp_; }
	
	inline void SetEngine(class NetBase* eng) { engine_ = eng; }
	inline class NetBase* GetEngine() const { return engine_; }
	
	inline void Disconnect() noexcept
	{
		bool expected = false;
		if (pending_disconnect_.compare_exchange_strong(expected, true))
		{
			CancelIoEx((HANDLE)sock_, NULL);
		}
	}
	
	template<typename T>
	bool SendPacket(const T& packet);
	
	// 비동기 송신 등록 (IOCPManager에서 이동)
	void PostAsyncSend();
	
	// 비동기 수신 등록 (IOCPManager에서 이동)
	bool PostAsyncReceive();
};
typedef Session* PSession;

// 템플릿 구현부
template<typename T>
inline bool Session::SendPacket(const T& packet)
{
    if (sock_ == INVALID_SOCKET || pending_disconnect_) 
    {
        return false;
    }
    
    // 1. 프로토버프 메시지 직렬화 크기 계산
    size_t payload_size = packet.ByteSizeLong();
    size_t total_size   = UNIFIED_HEADER_SIZE + payload_size;
    
    // 2. 패킷 ID 생성
    const std::string& type_name = packet.GetDescriptor()->full_name();
    uint32_t packet_id = fnv1a(type_name.c_str());
    
    // 3. 패킷 버퍼 생성
    std::vector<char>* packet_data = new std::vector<char>(total_size);
    
    // 4. 헤더 설정
    UnifiedPacketHeader* header = reinterpret_cast<UnifiedPacketHeader*>(packet_data->data());
    header->length = static_cast<uint32_t>(total_size);
    header->packet_id = packet_id;
    
    // 5. 페이로드 직렬화
    if (!packet.SerializeToArray(packet_data->data() + sizeof(UnifiedPacketHeader), payload_size))
    {
        delete packet_data;
        return false;
    }

	// 6. 송신 큐에 패킷 추가
	send_q_.Enqueue(packet_data);

	// 7. Send flag 체크 후 비동기 송신 시작
	if (InterlockedExchange8((char*)&send_flag_, true) == false)
	{
		PostAsyncSend();
	}

	return true;
}