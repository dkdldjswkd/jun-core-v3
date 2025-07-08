#pragma once
#include "NetCore.h"
#include "TestClientHandler.h"
#include <memory>
#include <string>

class TestClientManager
{
public:
    TestClientManager();

    bool Connect(const std::string& address, Port port);
    void Disconnect();
    void RunInteractiveSession();
    void RunAutomatedTest();

private:
    void ShowStatus();
    void SendPing();
    void SendMessage(const std::string& message);
    void ShowHelp();

    std::unique_ptr<INetworkClient> client_;
    TestClientHandler handler_;
};
