#pragma once
#include "Session.h"
#include <memory>

//------------------------------
// User - Session의 안전한 래퍼 클래스
// weak_ptr을 사용하여 Session 생명주기 관리
//------------------------------
class User
{
public:
    explicit User(std::weak_ptr<Session> session);
    ~User() = default;

    // 복사/이동 허용 (weak_ptr은 안전)
    User(const User&) = default;
    User& operator=(const User&) = default;
    User(User&&) = default;
    User& operator=(User&&) = default;

public:
    //------------------------------
    // 패킷 송신 인터페이스
    //------------------------------
    template<typename T>
    bool SendPacket(const T& packet);

    //------------------------------
    // 연결 상태 확인
    //------------------------------
    bool IsConnected() const;
    void Disconnect();

    //------------------------------
    // 세션 정보 접근
    //------------------------------
    std::string GetRemoteIP() const;
    WORD GetRemotePort() const;

private:
    std::weak_ptr<Session> session_;
};

//------------------------------
// 인라인 구현
//------------------------------

inline User::User(std::weak_ptr<Session> session) 
    : session_(session)
{
}

template<typename T>
inline bool User::SendPacket(const T& packet)
{
    if (auto session = session_.lock()) 
    {
        return session->SendPacket(packet);
    }
    return false;
}


inline bool User::IsConnected() const
{
    if (auto session = session_.lock()) 
    {
        return session->sock_ != INVALID_SOCKET && !session->pending_disconnect_;
    }
    return false;
}

inline void User::Disconnect()
{
    if (auto session = session_.lock()) 
    {
        session->Disconnect();
    }
}

inline std::string User::GetRemoteIP() const
{
    if (auto session = session_.lock()) 
    {
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &session->ip_, ip_str, sizeof(ip_str));
        return std::string(ip_str);
    }
    return "";
}

inline WORD User::GetRemotePort() const
{
    if (auto session = session_.lock()) 
    {
        return session->port_;
    }
    return 0;
}