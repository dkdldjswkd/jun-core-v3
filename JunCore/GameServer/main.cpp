#include "GameServerHandler.h"
#include "GameServerManager.h"
#include <iostream>

int main()
{
	try
	{
		// Configure logging
		auto console_logger = std::make_shared<ConsoleLogger>();
		console_logger->SetMinLevel(LogLevel::kInfo);
		Logger::SetLogger(console_logger);

		Logger::Info("JunCore3 Game Server starting...");

		// Create and start server
		GameServerManager server_manager;
		constexpr Port kServerPort = 9000;

		if (!server_manager.Start(kServerPort))
		{
			Logger::Error("Failed to start server on port " + std::to_string(kServerPort));
			return 1;
		}

		std::cout << "\n=== JunCore3 Game Server ===" << std::endl;
		std::cout << "Server is running on port " << kServerPort << std::endl;
		std::cout << "Local IP: " << NetworkUtils::GetLocalIPAddress() << std::endl;
		std::cout << "Type commands to interact with the server." << std::endl;

		// Run console interface
		server_manager.RunConsoleCommands();

		// Cleanup
		server_manager.Stop();
		Logger::Info("Server shutdown complete.");

	}
	catch (const std::exception& e)
	{
		Logger::Error("Server error: " + std::string(e.what()));
		return 1;
	}

	return 0;
}
