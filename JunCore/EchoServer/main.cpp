#include <iostream>
#include <thread>
#include <string>
#include "network/EchoServer.h"
#include "packet/packet.pb.h"

using namespace std;

int main()
{
    cout << "EchoServer using NetCore" << endl;
    cout << "Started at: " << __DATE__ << " " << __TIME__ << endl;

    // Test protobuf
    PacketLib::UG_ECHO_REQ test_packet;
    test_packet.set_echo("PacketLib::UG_ECHO_REQ - Test");
    cout << "Test packet: " << test_packet.echo() << endl;

    // Server connection settings
    std::string ip = "127.0.0.1";
    uint16 port = 8085;
    int32 workerThreads = 4;

    if (!sEchoServer.StartServer(ip, port, workerThreads))
    {
        cerr << "Failed to start EchoServer" << endl;
        return -1;
    }

    cout << "EchoServer is running. Type 'quit' to stop." << endl;

    // Server loop
    while (true)
    {
        std::string input;
        std::getline(std::cin, input);
        
        if (input == "quit" || input == "exit")
        {
            cout << "Shutting down server..." << endl;
            break;
        }
        else if (input == "status")
        {
            cout << "Server is running on " << ip << ":" << port << endl;
        }
        else if (!input.empty())
        {
            cout << "Unknown command. Type 'quit' to stop or 'status' for info." << endl;
        }
    }

    sEchoServer.StopServer();
    cout << "EchoServer terminated." << endl;
    return 0;
}