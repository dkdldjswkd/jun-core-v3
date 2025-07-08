#pragma once
#include "NetCore.h"

class TestClientHandler : public IClientEventHandler
{
public:
    void OnConnected(std::shared_ptr<IConnection> connection) override;
    void OnDisconnected(std::shared_ptr<IConnection> connection) override;
    void OnDataReceived(std::shared_ptr<IConnection> connection,
                        const char* data, size_t length) override;
    void OnError(std::shared_ptr<IConnection> connection,
                 NetworkError error, const std::string& message) override;

    // IClientEventHandler specific methods
    void OnConnectionAttempt(const std::string& address, Port port) override;
    void OnConnectionEstablished() override;
    void OnConnectionLost() override;
};
