#include "EchoServer.h"

EchoServer& EchoServer::Instance()
{
    static EchoServer _instance;
    return _instance;
}

bool EchoServer::StartServer(const std::string& ip, uint16 port, int workerThreads)
{
    InitPacketHandlers();
    
    server_manager_.OnAccept = [this](SessionPtr session_ptr) {
        OnAccept(session_ptr);
    };
    
    server_manager_.OnSessionClose = [this](SessionPtr session_ptr) {
        OnSessionClose(session_ptr);
    };
    
    
    if (!server_manager_.StartServer(ip, port, workerThreads))
    {
        std::cerr << "Failed to start EchoServer on " << ip << ":" << port << std::endl;
        return false;
    }
    
    std::cout << "EchoServer started on " << ip << ":" << port << std::endl;
    return true;
}

void EchoServer::StopServer()
{
    server_manager_.StopServer();
    std::cout << "EchoServer stopped." << std::endl;
}

void EchoServer::OnAccept(SessionPtr session_ptr)
{
    std::lock_guard<std::mutex> _lock(session_mutex_);
    session_set_.emplace(session_ptr);
    
    // Set up packet handling callback for this session
    session_ptr->OnPacketReceiveCallback = [this](SessionPtr session_ptr, int32 packet_id, const std::vector<char>& data) {
        server_manager_.HandlePacket(session_ptr, packet_id, data);
    };
    
    std::cout << "New client connected. Total clients: " << session_set_.size() << std::endl;
}

void EchoServer::OnSessionClose(SessionPtr session_ptr)
{
    std::lock_guard<std::mutex> _lock(session_mutex_);
    session_set_.erase(session_ptr);
    std::cout << "Client disconnected. Total clients: " << session_set_.size() << std::endl;
}

void EchoServer::InitPacketHandlers() 
{
    server_manager_.RegisterPacketHandler<PacketLib::UG_ECHO_REQ>(
        static_cast<int32>(PacketLib::PACKET_ID::PACKET_ID_UG_ECHO_REQ),
        [this](SessionPtr _session_ptr, const PacketLib::UG_ECHO_REQ& _packet) {
            std::cout << ">> Echo request received: " << _packet.echo() << std::endl;

            PacketLib::GU_ECHO_RES _res;
            _res.set_echo(_packet.echo());

            _session_ptr->SendPacket(static_cast<int32>(PacketLib::PACKET_ID::PACKET_ID_GU_ECHO_RES), _res);
            std::cout << "<< Echo response sent: " << _res.echo() << std::endl;
        }
    );
}