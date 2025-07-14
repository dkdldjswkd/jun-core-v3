#include "GameServerManager.h"
#include <iostream>

int main()
{
	GameServerManager server_manager;
	constexpr uint16 kServerPort = 9000;
	constexpr int kWorkerCount = 4; // Example worker count

	if (!server_manager.Start(kServerPort, kWorkerCount))
	{
		std::cerr << "Failed to start server on port " << kServerPort << std::endl;
		return 1;
	}

	std::cout << "
=== JunCore3 Game Server ===" << std::endl;
	std::cout << "Server is running on port " << kServerPort << std::endl;
	
	std::cout << "Type 'exit' to shutdown the server." << std::endl;

	server_manager.RunConsoleCommands();

	server_manager.Stop();
	std::cout << "Server shutdown complete." << std::endl;

	return 0;
}