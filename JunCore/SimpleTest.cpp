#include "EchoServer/EchoServer.h"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    try {
        std::cout << "Creating EchoServer..." << std::endl;
        EchoServer server("ServerConfig.ini", "SERVER");
        
        std::cout << "Starting server..." << std::endl;
        server.Start();
        
        std::cout << "Server started. Waiting 5 seconds..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        std::cout << "Stopping server..." << std::endl;
        server.Stop();
        
        std::cout << "Server stopped successfully." << std::endl;
        return 0;
    }
    catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
        return -1;
    }
}