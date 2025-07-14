#include "TestClientManager.h"
#include <iostream>
#include <string>
#include <vector>

// Global Winsock initialization flag (for simplicity, assuming one-time init)
extern bool g_winsock_initialized;

int main(int argc, char* argv[])
{
    // Initialize Winsock if not already done
    if (!g_winsock_initialized)
    {
        WSADATA wsaData;
        int ret = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (ret != 0)
        {
            std::cerr << "WSAStartup failed: " << ret << std::endl;
            return 1;
        }
        else
        {
            g_winsock_initialized = true;
        }
    }

    std::string server_address = "127.0.0.1";
    uint16 server_port = 9000;
    bool automated_test = false;

    if (argc >= 2)
    {
        server_address = argv[1];
    }
    if (argc >= 3)
    {
        server_port = static_cast<uint16>(std::stoi(argv[2]));
    }
    if (argc >= 4)
    {
        std::string mode = argv[3];
        if (mode == "auto")
        {
            automated_test = true;
        }
    }

    std::cout << "\n=== JunCore3 Test Client ===" << std::endl;
    std::cout << "Target server: " << server_address << ":" << server_port << std::endl;

    TestClientManager client_manager;

    if (!client_manager.Connect(server_address, server_port))
    {
        std::cerr << "Failed to connect to server" << std::endl;
        return 1;
    }

    if (automated_test)
    {
        client_manager.RunAutomatedTest();
    }
    else
    {
        client_manager.RunInteractiveSession();
    }

    client_manager.Disconnect();
    std::cout << "Client shutdown complete." << std::endl;

    // Cleanup Winsock if this is the last user (simplistic, consider ref counting)
    if (g_winsock_initialized)
    {
        WSACleanup();
        g_winsock_initialized = false;
    }

    return 0;
}
