#pragma once
#include "JobQueue.h"

// 전방 선언
class PacketBuffer;

//------------------------------
// PacketJobType - 패킷 처리 작업 유형
//------------------------------
enum class PacketJobType : BYTE
{
    RECV_PACKET = 1,        // OnRecv 호출
    CLIENT_JOIN = 2,        // OnClientJoin 호출  
    CLIENT_LEAVE = 3,       // OnClientLeave 호출
    CONNECTION_REQUEST = 4, // OnConnectionRequest 호출 (서버만)
    SEND_COMPLETE = 5,      // Send 완료 후처리 (필요시)
    CUSTOM = 99             // 사용자 정의 작업
};

//------------------------------
// SessionInfo - 세션 정보 (최소한의 데이터)
//------------------------------
struct SessionInfo
{
    union SessionId {
        struct {
            DWORD index;
            DWORD unique;
        };
        UINT64 sessionId;
    } sessionId;
    
    SOCKADDR_IN clientAddr = {};
    
    SessionInfo() { sessionId.sessionId = 0; }
    SessionInfo(DWORD index, DWORD unique) {
        sessionId.index = index;
        sessionId.unique = unique;
    }
};

//------------------------------
// PacketJob - 패킷 처리 전용 Job
//------------------------------
struct PacketJob
{
    PacketJobType jobType;
    SessionInfo sessionInfo;
    PacketBuffer* packet = nullptr;  // RECV_PACKET에서 사용
    void* userData = nullptr;        // 추가 데이터 (선택적)
    
    // Job 생성 헬퍼 함수들
    static PacketJob CreateRecvJob(SessionInfo sessionInfo, PacketBuffer* packet);
    static PacketJob CreateJoinJob(SessionInfo sessionInfo, SOCKADDR_IN clientAddr);
    static PacketJob CreateLeaveJob(SessionInfo sessionInfo);
    static PacketJob CreateConnectionRequestJob(SOCKADDR_IN clientAddr, void* userData = nullptr);
    
    // Job 함수로 변환
    Job ToJob(std::function<void(const PacketJob&)> handler, DWORD priority = 0) const;
};

//------------------------------
// PacketJobHandler - 패킷 Job 처리 인터페이스
//------------------------------
class PacketJobHandler
{
public:
    virtual ~PacketJobHandler() = default;
    
    // 순수 가상 함수 - 구현체에서 오버라이드
    virtual void HandleRecvPacket(SessionInfo sessionInfo, PacketBuffer* packet) = 0;
    virtual void HandleClientJoin(SessionInfo sessionInfo, SOCKADDR_IN clientAddr) = 0;
    virtual void HandleClientLeave(SessionInfo sessionInfo) = 0;
    virtual void HandleConnectionRequest(SOCKADDR_IN clientAddr, void* userData) {}  // 서버만 구현
    
    // Job 처리 메인 함수
    void HandlePacketJob(const PacketJob& job);
};

//------------------------------
// PacketJob 인라인 구현
//------------------------------

inline PacketJob PacketJob::CreateRecvJob(SessionInfo sessionInfo, PacketBuffer* packet)
{
    PacketJob job;
    job.jobType = PacketJobType::RECV_PACKET;
    job.sessionInfo = sessionInfo;
    job.packet = packet;
    return job;
}

inline PacketJob PacketJob::CreateJoinJob(SessionInfo sessionInfo, SOCKADDR_IN clientAddr)
{
    PacketJob job;
    job.jobType = PacketJobType::CLIENT_JOIN;
    job.sessionInfo = sessionInfo;
    job.sessionInfo.clientAddr = clientAddr;
    return job;
}

inline PacketJob PacketJob::CreateLeaveJob(SessionInfo sessionInfo)
{
    PacketJob job;
    job.jobType = PacketJobType::CLIENT_LEAVE;
    job.sessionInfo = sessionInfo;
    return job;
}

inline PacketJob PacketJob::CreateConnectionRequestJob(SOCKADDR_IN clientAddr, void* userData)
{
    PacketJob job;
    job.jobType = PacketJobType::CONNECTION_REQUEST;
    job.sessionInfo.clientAddr = clientAddr;
    job.userData = userData;
    return job;
}

inline Job PacketJob::ToJob(std::function<void(const PacketJob&)> handler, DWORD priority) const
{
    return Job([handler, job = *this]() {
        handler(job);
    }, priority);
}

//------------------------------
// PacketJobHandler 구현
//------------------------------

inline void PacketJobHandler::HandlePacketJob(const PacketJob& job)
{
    switch (job.jobType)
    {
        case PacketJobType::RECV_PACKET:
            HandleRecvPacket(job.sessionInfo, job.packet);
            break;
            
        case PacketJobType::CLIENT_JOIN:
            HandleClientJoin(job.sessionInfo, job.sessionInfo.clientAddr);
            break;
            
        case PacketJobType::CLIENT_LEAVE:
            HandleClientLeave(job.sessionInfo);
            break;
            
        case PacketJobType::CONNECTION_REQUEST:
            HandleConnectionRequest(job.sessionInfo.clientAddr, job.userData);
            break;
            
        default:
            // 알 수 없는 Job 타입 - 무시
            break;
    }
}