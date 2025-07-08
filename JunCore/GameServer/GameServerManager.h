#pragma once
#include "NetCore.h"
#include "GameServerHandler.h"
#include <memory>
#include <string>

class GameServerManager
{
public:
    GameServerManager();

    bool Start(Port port);
    void Stop();
    void RunConsoleCommands();

private:
    void ShowStatus();
    void ShowClients();
    void BroadcastMessage(const std::string& message);

    std::unique_ptr<INetworkServer> server_;
    GameServerHandler handler_;
};
