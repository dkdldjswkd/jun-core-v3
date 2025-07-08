#pragma once
#include "NetCore.h"
#include <atomic>

class GameServerHandler : public IServerEventHandler
{
public:
    // INetworkEventHandler interface
    void OnConnected(std::shared_ptr<IConnection> connection) override;
    void OnDisconnected(std::shared_ptr<IConnection> connection) override;
    void OnDataReceived(std::shared_ptr<IConnection> connection,
                        const char* data, size_t length) override;
    void OnError(std::shared_ptr<IConnection> connection,
                 NetworkError error, const std::string& message) override;

    // IServerEventHandler interface
    void OnClientAccepted(std::shared_ptr<IConnection> client) override;

private:
    std::atomic<size_t> total_clients_served_{0};
};
