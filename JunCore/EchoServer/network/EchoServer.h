#pragma once
#include <network/ServerManager.h>
#include "../packet/packet.pb.h"
#include <iostream>
#include <mutex>
#include <set>

class EchoServer
{
public:
    static EchoServer& Instance();

public:
    bool StartServer(const std::string& ip, uint16 port, int workerThreads);
    void StopServer();

private:
    void OnAccept(SessionPtr session_ptr);
    void OnSessionClose(SessionPtr session_ptr);
    void InitPacketHandlers();

private:
    ServerManager server_manager_;
    std::mutex session_mutex_;
    std::set<SessionPtr> session_set_;
};

#define sEchoServer EchoServer::Instance()