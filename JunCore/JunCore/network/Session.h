#pragma once
#include "../core/WindowsIncludes.h"
#include "../../JunCommon/container/LFQueue.h"
#include "../../JunCommon/container/LFStack.h"
#include "../../JunCommon/container/RingBuffer.h"
#include "../core/base.h"
#include "../protocol/UnifiedPacketHeader.h"
#include <vector>
#include <string>
#include "IOCPManager.h"

constexpr int MAX_SEND_MSG = 100;
constexpr int INVALID_SESSION_ID = -1;

//------------------------------
// SessionId
//------------------------------
union SessionId
{
public:
	struct { DWORD index, unique; } s;
	DWORD64	sessionId = 0;
#define		SESSION_INDEX  s.index   
#define		SESSION_UNIQUE s.unique

public:
	SessionId();
	SessionId(DWORD64 value);
	SessionId(DWORD index, DWORD unique_no);
	~SessionId();

public:
	void operator=(const SessionId& other);
	void operator=(DWORD64 value);
	operator DWORD64();
};

//------------------------------
// Session
//------------------------------
class IOCPManager; // Forward declaration

class Session
{
	friend class IOCPManager;

public:
	Session();
	~Session();

public:
	// 소켓 정보
	SOCKET sock_ = INVALID_SOCKET;
	in_addr ip_;
	WORD port_;
	SessionId session_id_ = INVALID_SESSION_ID;

	// flag
	bool send_flag_ = false;
	bool disconnect_flag_ = false;

	// Send
	LFQueue<std::vector<char>*> send_q_;			// 송신 대기 큐 (raw 데이터)
	std::vector<char>* send_packet_arr_[MAX_SEND_MSG];	// 현재 전송중인 패킷 벡터들
	LONG send_packet_count_ = 0;					// 현재 전송중인 패킷 개수

	// Recv
	RingBuffer recv_buf_;

	// TimeOut
	DWORD last_recv_time_;

	// Overlapped
	OVERLAPPED recv_overlapped_ = { 0, };
	OVERLAPPED send_overlapped_ = { 0, };

	// 암호화
	std::vector<char> aes_key_;

	// 세션 해제여부와 카운트 관리 (release_flag_, io_count_를 8byte로 정렬, false 캐시라인과 분리되게 유도)
	alignas(64) BOOL release_flag_ = true;
	LONG io_count_ = 0;

private:
	HANDLE h_iocp_ = INVALID_HANDLE_VALUE;  // IOCP 핸들 저장
	class NetBase* engine_ = nullptr;       // 이 세션을 소유한 엔진
	
	// Session Pool 직접 관리
	std::vector<Session>* session_pool_ = nullptr;  // 자신이 속한 세션 풀
	LFStack<DWORD>* session_index_stack_ = nullptr; // 세션 인덱스 스택

public:
	void Set(SOCKET sock, in_addr ip, WORD port, SessionId session_id, NetBase* eng);
	void Release();  // 세션 정리 + Pool 반환 통합
	
	// Session Pool 직접 설정 (간단하고 명확함)
	void SetSessionPool(std::vector<Session>* pool, LFStack<DWORD>* indexStack);
	
	inline void SetIOCP(HANDLE iocp_handle) { h_iocp_ = iocp_handle; }
	inline HANDLE GetIOCP() { return h_iocp_; }
	
	inline void SetEngine(class NetBase* eng) { engine_ = eng; }
	inline class NetBase* GetEngine() const { return engine_; }
	
	__forceinline void IncrementIOCount() noexcept
	{
		InterlockedIncrement(&io_count_);
	}
	
	__forceinline void DecrementIOCount() noexcept
	{
		if (0 == InterlockedDecrement(&io_count_))
		{
			// * release_flag_(0), IOCount(0) -> release_flag_(1), IOCount(0)
			if (0 == InterlockedCompareExchange64((long long*)&release_flag_, 1, 0))
			{
				Release();
			}
		}
	}
	
	// 연결 해제
	inline void DisconnectSession() noexcept
	{
		disconnect_flag_ = true;
		CancelIoEx((HANDLE)sock_, NULL);
	}
	
	// 패킷 송신 (NetBase에서 이동)
	bool SendPacket(std::vector<char>* packet_data);
	
	// 템플릿 패킷 송신 (Protobuf 메시지)
	template<typename T>
	bool SendPacket(const T& packet);
	
	// 비동기 송신 등록 (IOCPManager에서 이동)
	void PostAsyncSend();
};
typedef Session* PSession;

// 템플릿 구현부
template<typename T>
inline bool Session::SendPacket(const T& packet)
{
    // 세션 유효성 검사
    if (sock_ == INVALID_SOCKET || disconnect_flag_) 
    {
        return false;
    }
    
    // 1. 프로토버프 메시지 직렬화 크기 계산
    size_t payload_size = packet.ByteSizeLong();
    size_t total_size   = UNIFIED_HEADER_SIZE + payload_size;
    
    // 2. 패킷 ID 생성 (프로토버프 타입명 해시)
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
        delete packet_data;  // 실패 시 메모리 해제
        return false;
    }
    
    // 6. 기존 SendPacket(raw data)로 전달
    return SendPacket(packet_data);
}