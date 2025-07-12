#include "NetworkFactory.h"

//==============================================================================
// NetworkFactory Implementation
//==============================================================================

std::unique_ptr<INetworkServer> NetworkFactory::CreateServer(
	std::unique_ptr<IConnectionManager> connection_manager)
{
	return std::make_unique<NetworkServer>(std::move(connection_manager));
}

std::unique_ptr<INetworkClient> NetworkFactory::CreateClient()
{
	return std::make_unique<NetworkClient>();
}

std::unique_ptr<IConnectionManager> NetworkFactory::CreateConnectionManager()
{
	return std::make_unique<ConnectionManager>();
}